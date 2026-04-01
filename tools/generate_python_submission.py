#!/usr/bin/env python3
"""Generate a single-file Python bot submission for CodinGame.

Takes trained model weights (JSON) and a Python bot template, embeds the
encoded weights, optionally minifies, and verifies the result fits within
CodinGame's 100K character limit.

Usage:
    python3 tools/generate_python_submission.py \\
        --weights rust/bot/weights/distill02_32ch_3L.json \\
        --bot python/bot/bot.py \\
        --output submission/python_bot_32ch.py
"""
from __future__ import annotations

import argparse
import ast
import json
import re
import struct
import subprocess
import sys
import textwrap
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CHAR_LIMIT = 100_000


# ---------------------------------------------------------------------------
# Weight encoding — matches Rust submission generator exactly
# ---------------------------------------------------------------------------

def _collect_weight_bytes(model: dict, *, dtype: str = "f16") -> bytes:
    """Pack all weight/bias floats into a byte buffer.

    Order: conv1.w, conv1.b, conv2.w, conv2.b,
           [conv3.w, conv3.b], policy.w, policy.b, value.w, value.b

    dtype: "f32" for full precision, "f16" for half precision.
    """
    import numpy as np

    layer_order = ["conv1", "conv2"]
    if model.get("conv3") is not None:
        layer_order.append("conv3")
    layer_order.extend(["policy", "value"])

    all_values: list[float] = []
    for layer_name in layer_order:
        layer = model[layer_name]
        for array_name in ("weights", "bias"):
            all_values.extend(layer[array_name])

    arr = np.array(all_values, dtype=np.float32)
    if dtype == "f16":
        arr = arr.astype(np.float16)
    return arr.tobytes()


def _bytes_to_unicode(buf: bytes) -> str:
    """Encode raw bytes as Unicode BMP characters (2 bytes per char).

    Surrogate codepoints (U+D800..U+DFFF) are escaped using U+FFFF sentinel:
      - u16 in 0xD800..0xDFFF  ->  chr(0xFFFF) + chr(u16 - 0xD800)
      - u16 == 0xFFFF           ->  chr(0xFFFF) + chr(0x0800)
      - all others              ->  chr(u16) directly
    """
    if len(buf) % 2 != 0:
        buf += b"\x00"
    ESCAPE = 0xFFFF
    chars: list[str] = []
    for i in range(0, len(buf), 2):
        u16_val = buf[i] | (buf[i + 1] << 8)  # little-endian
        if 0xD800 <= u16_val <= 0xDFFF:
            chars.append(chr(ESCAPE))
            chars.append(chr(u16_val - 0xD800))
        elif u16_val == ESCAPE:
            chars.append(chr(ESCAPE))
            chars.append(chr(0x0800))
        else:
            chars.append(chr(u16_val))
    return "".join(chars)


def encode_weights_f16_unicode(model: dict) -> str:
    """Encode weights as f16 Unicode BMP (1 char per weight, ~5.3x denser than base64)."""
    return _bytes_to_unicode(_collect_weight_bytes(model, dtype="f16"))


def count_weight_params(model: dict) -> int:
    """Count total number of weight parameters."""
    layer_order = ["conv1", "conv2"]
    if model.get("conv3") is not None:
        layer_order.append("conv3")
    layer_order.extend(["policy", "value"])

    total = 0
    for layer_name in layer_order:
        layer = model[layer_name]
        total += len(layer["weights"]) + len(layer["bias"])
    return total


# ---------------------------------------------------------------------------
# Python string escaping for embedding
# ---------------------------------------------------------------------------

def _python_unicode_literal(s: str) -> str:
    """Convert a Unicode string to a safe Python string literal (double-quoted).

    Escapes control characters, backslash, double-quote, and non-printable
    characters using \\uXXXX or \\xXX notation. All other BMP codepoints are
    written as raw UTF-8.
    """
    parts: list[str] = []
    for ch in s:
        cp = ord(ch)
        if cp == 0x5C:  # backslash
            parts.append("\\\\")
        elif cp == 0x22:  # double quote
            parts.append('\\"')
        elif cp == 0x0A:  # newline
            parts.append("\\n")
        elif cp == 0x0D:  # carriage return
            parts.append("\\r")
        elif cp == 0x09:  # tab
            parts.append("\\t")
        elif cp == 0x00:  # null
            parts.append("\\x00")
        elif cp < 0x20 or cp == 0x7F:  # other control chars
            parts.append(f"\\x{cp:02x}")
        else:
            parts.append(ch)
    return '"' + "".join(parts) + '"'


# ---------------------------------------------------------------------------
# Model config extraction
# ---------------------------------------------------------------------------

def extract_model_config(model: dict) -> dict:
    """Extract MODEL_CONFIG dict from weights JSON.

    Must include all keys expected by bot.py's load_model():
      conv_channels, num_conv_layers, input_channels, scalar_features,
      birds_per_player, actions_per_bird
    """
    conv_channels = model["conv1"]["out_channels"]
    num_conv_layers = 3 if model.get("conv3") is not None else 2
    input_channels = model.get("input_channels", model["conv1"]["in_channels"])
    scalar_features = model.get("scalar_features", 6)
    policy_outputs = model["policy"]["out_features"]  # = birds * actions
    birds_per_player = 4
    actions_per_bird = 5
    if policy_outputs != birds_per_player * actions_per_bird:
        # Try to infer from policy output size
        for b in (4, 3, 2):
            if policy_outputs % b == 0:
                birds_per_player = b
                actions_per_bird = policy_outputs // b
                break

    return {
        "conv_channels": conv_channels,
        "num_conv_layers": num_conv_layers,
        "input_channels": input_channels,
        "scalar_features": scalar_features,
        "birds_per_player": birds_per_player,
        "actions_per_bird": actions_per_bird,
    }


def model_desc(model: dict) -> str:
    """Derive a short model description like '12ch/3L'."""
    conv_channels = model.get("conv1", {}).get("out_channels", "?")
    num_layers = 3 if model.get("conv3") is not None else 2
    return f"{conv_channels}ch/{num_layers}L"


# ---------------------------------------------------------------------------
# Bot template embedding
# ---------------------------------------------------------------------------

def embed_weights_in_template(
    template: str,
    weights_literal: str,
    model_config: dict,
) -> str:
    """Replace placeholder patterns in the bot template with actual values.

    Expects the template to contain:
      WEIGHTS_F16 = ""
      MODEL_CONFIG = {... (anything on that line and possibly following lines until })
    """
    # Replace WEIGHTS_F16 = "" — use string find/replace to avoid regex escaping issues
    # with Unicode weight literals that contain backslash sequences
    for quote in ['""', "''"]:
        marker = f'WEIGHTS_F16 = {quote}'
        pos = template.find(marker)
        if pos >= 0:
            template = template[:pos] + f'WEIGHTS_F16 = {weights_literal}' + template[pos + len(marker):]
            break
    else:
        print("ERROR: Bot template does not contain 'WEIGHTS_F16 = \"\"' pattern",
              file=sys.stderr)
        sys.exit(1)

    # Replace MODEL_CONFIG = { ... }
    config_str = json.dumps(model_config, separators=(", ", ": "))
    config_marker = "MODEL_CONFIG = {"
    pos = template.find(config_marker)
    if pos >= 0:
        # Find the closing brace
        brace_depth = 0
        for i in range(pos + len(config_marker) - 1, len(template)):
            if template[i] == '{':
                brace_depth += 1
            elif template[i] == '}':
                brace_depth -= 1
                if brace_depth == 0:
                    template = template[:pos] + f'MODEL_CONFIG = {config_str}' + template[i + 1:]
                    break
    else:
        print("WARNING: Bot template does not contain 'MODEL_CONFIG = {' pattern. "
              "Skipping config embedding.", file=sys.stderr)

    return template


# ---------------------------------------------------------------------------
# Minification
# ---------------------------------------------------------------------------

def minify_python(source: str) -> str:
    """Remove comments, docstrings, blank lines, and trailing whitespace.

    Preserves:
      - Shebang lines
      - Indentation structure
      - String contents
      - Inline comments that are part of meaningful lines are kept
        (only pure comment lines are removed)
    """
    lines = source.split("\n")
    result: list[str] = []

    in_docstring = False
    docstring_char = None
    docstring_indent = 0

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Handle ongoing docstring
        if in_docstring:
            if docstring_char in stripped:
                # Check if this line closes the docstring
                # Count occurrences of the triple-quote in this line
                close_pos = stripped.find(docstring_char)
                if close_pos >= 0:
                    in_docstring = False
                    i += 1
                    continue
            i += 1
            continue

        # Check for docstring start: a line that is ONLY a triple-quote string
        # or starts a triple-quote string (common patterns: """ or ''')
        if stripped.startswith('"""') or stripped.startswith("'''"):
            quote_char = stripped[:3]
            # Check if it's a standalone docstring (opens and closes, or opens multi-line)
            rest_after_open = stripped[3:]
            if quote_char in rest_after_open:
                # Single-line docstring like """text""" — skip entire line
                i += 1
                continue
            else:
                # Multi-line docstring starts here
                in_docstring = True
                docstring_char = quote_char
                docstring_indent = len(line) - len(line.lstrip())
                i += 1
                continue

        # Skip blank lines
        if not stripped:
            i += 1
            continue

        # Skip pure comment lines (but not shebang)
        if stripped.startswith("#") and not stripped.startswith("#!"):
            i += 1
            continue

        # Keep the line, strip trailing whitespace
        result.append(line.rstrip())
        i += 1

    return "\n".join(result) + "\n"


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------

def verify_compile(path: Path) -> bool:
    """Check that the output file is valid Python by compiling it."""
    try:
        proc = subprocess.run(
            [
                sys.executable,
                "-c",
                f"compile(open({str(path)!r}).read(), {str(path)!r}, 'exec')",
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if proc.returncode != 0:
            print(f"COMPILE CHECK FAILED:\n{proc.stderr}", file=sys.stderr)
            return False
        return True
    except subprocess.TimeoutExpired:
        print("COMPILE CHECK TIMED OUT", file=sys.stderr)
        return False


def verify_ast_parse(path: Path) -> bool:
    """Verify the output file parses as valid Python AST."""
    try:
        source = path.read_text(encoding="utf-8")
        ast.parse(source)
        return True
    except SyntaxError as e:
        print(f"AST PARSE FAILED: {e}", file=sys.stderr)
        return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a Python bot submission with embedded neural network weights.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Example:
                python3 tools/generate_python_submission.py \\
                    --weights rust/bot/weights/distill02_32ch_3L.json \\
                    --bot python/bot/bot.py \\
                    --output submission/python_bot_32ch.py
        """),
    )
    parser.add_argument(
        "--weights",
        type=Path,
        required=True,
        help="Path to weights JSON file (same format as rust/bot/weights/*.json)",
    )
    parser.add_argument(
        "--bot",
        type=Path,
        required=True,
        help="Path to Python bot template file (must contain WEIGHTS_F16 = \"\" placeholder)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output path for the combined submission file "
        "(default: submission/python_bot_<model>.py)",
    )
    parser.add_argument(
        "--no-minify",
        action="store_true",
        help="Skip whitespace/comment stripping",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="Skip compile check",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    # Resolve paths relative to repo root if not absolute
    weights_path: Path = args.weights
    if not weights_path.is_absolute():
        weights_path = REPO_ROOT / weights_path

    bot_path: Path = args.bot
    if not bot_path.is_absolute():
        bot_path = REPO_ROOT / bot_path

    # Load weights
    if not weights_path.exists():
        print(f"ERROR: Weights file not found: {weights_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Loading weights from {weights_path}")
    with open(weights_path, encoding="utf-8") as f:
        model = json.load(f)

    desc = model_desc(model)
    param_count = count_weight_params(model)
    print(f"Model: {desc} ({param_count:,} parameters)")

    # Load bot template
    if not bot_path.exists():
        print(f"ERROR: Bot template not found: {bot_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Loading bot template from {bot_path}")
    template = bot_path.read_text(encoding="utf-8")

    # Encode weights
    print("Encoding weights as f16 Unicode...")
    encoded = encode_weights_f16_unicode(model)
    weight_chars = len(encoded)
    # Account for escape overhead in the Python string literal
    weights_literal = _python_unicode_literal(encoded)
    weight_literal_chars = len(weights_literal)
    print(f"  Raw weight chars: {weight_chars:,}")
    print(f"  String literal chars (with escapes): {weight_literal_chars:,}")

    # Extract model config
    config = extract_model_config(model)
    print(f"  Model config: {json.dumps(config)}")

    # Embed into template
    print("Embedding weights into bot template...")
    combined = embed_weights_in_template(template, weights_literal, config)

    # Minify
    if not args.no_minify:
        print("Minifying...")
        pre_minify = len(combined)
        combined = minify_python(combined)
        post_minify = len(combined)
        print(f"  Before: {pre_minify:,} chars")
        print(f"  After:  {post_minify:,} chars")
        print(f"  Saved:  {pre_minify - post_minify:,} chars "
              f"({(pre_minify - post_minify) / pre_minify * 100:.1f}%)")
    else:
        print("Skipping minification (--no-minify)")

    # Determine output path
    output_path: Path = args.output if args.output else None
    if output_path is None:
        output_path = REPO_ROOT / "submission" / f"python_bot_{desc.replace('/', '_')}.py"
    elif not output_path.is_absolute():
        output_path = REPO_ROOT / output_path

    # Character budget check
    total_chars = len(combined)
    code_chars = total_chars - weight_literal_chars

    print()
    print("=" * 60)
    print("SUBMISSION SUMMARY")
    print("=" * 60)
    print(f"  Model:         {desc}")
    print(f"  Parameters:    {param_count:,}")
    print(f"  Weight chars:  {weight_literal_chars:,}")
    print(f"  Code chars:    {code_chars:,}")
    print(f"  Total chars:   {total_chars:,}")
    print(f"  Char limit:    {CHAR_LIMIT:,}")
    print(f"  Remaining:     {CHAR_LIMIT - total_chars:,}")
    print(f"  Usage:         {total_chars / CHAR_LIMIT * 100:.1f}%")
    print("=" * 60)

    if total_chars > CHAR_LIMIT:
        overage = total_chars - CHAR_LIMIT
        print(
            f"\nFATAL: Submission exceeds 100K character limit by {overage:,} chars!",
            file=sys.stderr,
        )
        print(
            f"  Need to reduce by {overage:,} chars. Consider:",
            file=sys.stderr,
        )
        print(
            f"    - Using a smaller model (fewer channels or layers)",
            file=sys.stderr,
        )
        print(
            f"    - Further code minification",
            file=sys.stderr,
        )
        sys.exit(1)

    # Write output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(combined, encoding="utf-8")
    print(f"\nWrote submission to {output_path}")

    # Verify
    if not args.no_verify:
        print("Running compile check...")
        if verify_compile(output_path) and verify_ast_parse(output_path):
            print("Compile check PASSED")
        else:
            print("Compile check FAILED", file=sys.stderr)
            sys.exit(1)
    else:
        print("Skipping compile check (--no-verify)")

    print("\nDone!")


if __name__ == "__main__":
    main()
