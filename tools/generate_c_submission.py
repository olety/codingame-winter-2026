#!/usr/bin/env python3
"""Generate a single-file C bot submission for CodinGame with int4 weights.

Takes int4 weights JSON and a C bot template, embeds the encoded weights,
and verifies the result fits within CodinGame's 100K character limit.

Usage:
    python3 tools/generate_c_submission.py \
        --weights path/to/weights_int4.json \
        --bot c_bot/bot.c \
        --output submission/c_bot_96ch.c

    # With config overrides:
    python3 tools/generate_c_submission.py \
        --weights path/to/weights_int4.json \
        --bot c_bot/bot.c \
        --output submission/c_bot_96ch.c \
        --prior-mix 0.0 --leaf-mix 0.08 --value-scale 48.0
"""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
import textwrap
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[1]
CHAR_LIMIT = 100_000


# ---------------------------------------------------------------------------
# Int4 weight encoding — 4 int4 values per Unicode char
# ---------------------------------------------------------------------------

def encode_int4_unicode(weights_int4: list[int]) -> str:
    """Pack int4 values into Unicode BMP chars. 4 int4 → 2 bytes → 1 char.

    Each int4 value is in [-8, 7]. Stored as 4-bit twos-complement nibble.
    Pack 4 nibbles into 2 bytes (little-endian), encode as Unicode codepoint.
    Surrogate handling identical to f16 encoding.
    """
    ESCAPE = 0xFFFF
    chars: list[str] = []
    for i in range(0, len(weights_int4), 4):
        vals = weights_int4[i:i + 4]
        nibbles = [v & 0xF for v in vals]
        # Pad with zeros if less than 4 values remaining
        while len(nibbles) < 4:
            nibbles.append(0)
        byte0 = nibbles[0] | (nibbles[1] << 4)
        byte1 = nibbles[2] | (nibbles[3] << 4)
        u16 = byte0 | (byte1 << 8)
        if 0xD800 <= u16 <= 0xDFFF:
            chars.append(chr(ESCAPE))
            chars.append(chr(u16 - 0xD800))
        elif u16 == ESCAPE:
            chars.append(chr(ESCAPE))
            chars.append(chr(0x0800))
        else:
            chars.append(chr(u16))
    return "".join(chars)


def encode_f16_unicode(values: list[float]) -> str:
    """Encode float values as f16 Unicode BMP chars (1 char per value)."""
    arr = np.array(values, dtype=np.float32).astype(np.float16)
    buf = arr.tobytes()
    ESCAPE = 0xFFFF
    chars: list[str] = []
    for i in range(0, len(buf), 2):
        u16 = buf[i] | (buf[i + 1] << 8)
        if 0xD800 <= u16 <= 0xDFFF:
            chars.append(chr(ESCAPE))
            chars.append(chr(u16 - 0xD800))
        elif u16 == ESCAPE:
            chars.append(chr(ESCAPE))
            chars.append(chr(0x0800))
        else:
            chars.append(chr(u16))
    return "".join(chars)


def encode_f32_as_bytes_unicode(values: list[float]) -> str:
    """Encode float32 values as raw bytes → Unicode BMP chars (2 chars per value)."""
    arr = np.array(values, dtype=np.float32)
    buf = arr.tobytes()
    ESCAPE = 0xFFFF
    chars: list[str] = []
    for i in range(0, len(buf), 2):
        u16 = buf[i] | (buf[i + 1] << 8)
        if 0xD800 <= u16 <= 0xDFFF:
            chars.append(chr(ESCAPE))
            chars.append(chr(u16 - 0xD800))
        elif u16 == ESCAPE:
            chars.append(chr(ESCAPE))
            chars.append(chr(0x0800))
        else:
            chars.append(chr(u16))
    return "".join(chars)


# ---------------------------------------------------------------------------
# C string literal escaping
# ---------------------------------------------------------------------------

def c_string_literal(s: str) -> str:
    """Convert a Unicode string to a safe C string literal.

    Most BMP codepoints are written as raw UTF-8 characters.
    Control characters and special chars are escaped.
    """
    parts: list[str] = []
    for ch in s:
        cp = ord(ch)
        if cp == 0x5C:  # backslash
            parts.append("\\\\")
        elif cp == 0x22:  # double quote
            parts.append('\\"')
        elif cp == 0x0A:
            parts.append("\\n")
        elif cp == 0x0D:
            parts.append("\\r")
        elif cp == 0x09:
            parts.append("\\t")
        elif cp == 0x00:
            parts.append("\\0")
        elif cp < 0x20 or cp == 0x7F:
            parts.append(f"\\x{cp:02x}")
        elif cp == 0x3F:  # question mark — avoid trigraphs
            parts.append("?")
        else:
            parts.append(ch)
    return '"' + "".join(parts) + '"'


# ---------------------------------------------------------------------------
# Weight data assembly
# ---------------------------------------------------------------------------

def build_weight_data(model: dict) -> dict:
    """Build encoded weight strings for embedding in C template.

    Returns dict with keys like CONV1_W_DATA, CONV1_S_DATA, CONV1_B_DATA, etc.
    """
    layer_order = ["conv1", "conv2"]
    if model.get("conv3") is not None:
        layer_order.append("conv3")
    layer_order.extend(["policy", "value"])

    result = {}
    total_int4 = 0
    total_scale = 0
    total_bias = 0

    for layer_name in layer_order:
        layer = model[layer_name]
        prefix = layer_name.upper()

        # Int4 weights
        w_int4 = layer["weights_int4"]
        encoded_w = encode_int4_unicode(w_int4)
        result[f"{prefix}_W_DATA"] = encoded_w
        result[f"{prefix}_W_COUNT"] = len(w_int4)
        total_int4 += len(encoded_w)

        # Per-channel scales (f32 — small count, precision matters)
        scales = layer["weight_scales"]
        encoded_s = encode_f32_as_bytes_unicode(scales)
        result[f"{prefix}_S_DATA"] = encoded_s
        result[f"{prefix}_S_COUNT"] = len(scales)
        total_scale += len(encoded_s)

        # Biases (f32)
        biases = layer["bias"]
        encoded_b = encode_f32_as_bytes_unicode(biases)
        result[f"{prefix}_B_DATA"] = encoded_b
        result[f"{prefix}_B_COUNT"] = len(biases)
        total_bias += len(encoded_b)

    result["_total_int4_chars"] = total_int4
    result["_total_scale_chars"] = total_scale
    result["_total_bias_chars"] = total_bias
    return result


# ---------------------------------------------------------------------------
# C code generation for weight decoding
# ---------------------------------------------------------------------------

def generate_weight_decoder_c() -> str:
    """Generate C functions for decoding Unicode-encoded weights."""
    return """
/* --- Weight decoder --- */
static int _next_cp(const char **p) {
    unsigned char c = (unsigned char)**p; (*p)++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) { int r = (c & 0x1F) << 6; r |= (*(*p)++ & 0x3F); return r; }
    int r = (c & 0x0F) << 12; r |= (*(*p)++ & 0x3F) << 6; r |= (*(*p)++ & 0x3F); return r;
}
static int _next_u16(const char **p) {
    int cp = _next_cp(p);
    if (cp == 0xFFFF) { int cp2 = _next_cp(p); return (cp2 == 0x0800) ? 0xFFFF : cp2 + 0xD800; }
    return cp;
}
static void decode_int4(const char *data, int8_t *out, int count) {
    const char *p = data;
    int w = 0;
    while (w < count) {
        int u = _next_u16(&p);
        int b0 = u & 0xFF, b1 = (u >> 8) & 0xFF;
        int n0 = b0 & 0xF, n1 = (b0 >> 4) & 0xF, n2 = b1 & 0xF, n3 = (b1 >> 4) & 0xF;
        out[w++] = (int8_t)((n0 & 8) ? (n0 | 0xF0) : n0); if (w >= count) break;
        out[w++] = (int8_t)((n1 & 8) ? (n1 | 0xF0) : n1); if (w >= count) break;
        out[w++] = (int8_t)((n2 & 8) ? (n2 | 0xF0) : n2); if (w >= count) break;
        out[w++] = (int8_t)((n3 & 8) ? (n3 | 0xF0) : n3);
    }
}
static void decode_f32(const char *data, float *out, int count) {
    const char *p = data;
    /* 2 u16 values per f32 */
    for (int i = 0; i < count; i++) {
        int lo = _next_u16(&p);
        int hi = _next_u16(&p);
        unsigned int bits = (unsigned int)lo | ((unsigned int)hi << 16);
        memcpy(&out[i], &bits, 4);
    }
}
"""


# ---------------------------------------------------------------------------
# Template embedding
# ---------------------------------------------------------------------------

def embed_in_template(
    template: str,
    weight_data: dict,
    model: dict,
    config_overrides: dict,
) -> str:
    """Replace WEIGHTS_PLACEHOLDER section in C bot template with weight data.

    Expected markers:
      // WEIGHTS_PLACEHOLDER_START
      ... (everything between is replaced)
      // WEIGHTS_PLACEHOLDER_END
    """
    layer_order = ["conv1", "conv2"]
    has_conv3 = model.get("conv3") is not None
    if has_conv3:
        layer_order.append("conv3")
    layer_order.extend(["policy", "value"])

    conv_channels = model["conv1"]["out_channels"]

    # Config overrides
    eval_cfg = {
        "EVAL_BODY": 120.0, "EVAL_LOSS": 18.0, "EVAL_MOBILITY": 7.5, "EVAL_APPLE": 16.0,
        "EVAL_STABILITY": 10.0, "EVAL_BREAKPOINT": 9.0, "EVAL_FRAGILE": 8.0, "EVAL_TERMINAL": 10000.0,
    }
    hybrid_cfg = {
        "PRIOR_MIX": 0.0, "LEAF_MIX": 0.08, "VALUE_SCALE": 48.0,
        "PRIOR_DEPTH_LIMIT": 0, "LEAF_DEPTH_LIMIT": 1,
    }
    search_cfg = {
        "SEARCH_FIRST_MS": 850, "SEARCH_LATER_MS": 40,
        "DEEPEN_TOP_MY": 6, "DEEPEN_TOP_OPP": 6,
        "DEEPEN_CHILD_MY": 4, "DEEPEN_CHILD_OPP": 4,
    }
    # Apply overrides (convert snake_case keys to matching config keys)
    override_map = {
        "prior_mix": "PRIOR_MIX", "leaf_mix": "LEAF_MIX", "value_scale": "VALUE_SCALE",
        "prior_depth_limit": "PRIOR_DEPTH_LIMIT", "leaf_depth_limit": "LEAF_DEPTH_LIMIT",
        "body": "EVAL_BODY", "loss": "EVAL_LOSS", "mobility": "EVAL_MOBILITY",
        "apple": "EVAL_APPLE", "stability": "EVAL_STABILITY", "breakpoint": "EVAL_BREAKPOINT",
        "fragile_attack": "EVAL_FRAGILE", "terminal": "EVAL_TERMINAL",
        "first_turn_ms": "SEARCH_FIRST_MS", "later_turn_ms": "SEARCH_LATER_MS",
    }
    all_cfg = {**eval_cfg, **hybrid_cfg, **search_cfg}
    for k, v in config_overrides.items():
        target = override_map.get(k, k.upper())
        if target in all_cfg:
            all_cfg[target] = v

    # Build the weight section
    s: list[str] = []
    s.append(generate_weight_decoder_c())

    # Encoded weight strings
    for layer_name in layer_order:
        prefix = layer_name.upper()
        s.append(f"static const char {prefix}_W_ENC[] = {c_string_literal(weight_data[f'{prefix}_W_DATA'])};")
        s.append(f"static const char {prefix}_S_ENC[] = {c_string_literal(weight_data[f'{prefix}_S_DATA'])};")
        s.append(f"static const char {prefix}_B_ENC[] = {c_string_literal(weight_data[f'{prefix}_B_DATA'])};")

    # init_weights function
    s.append("static void init_weights(void) {")
    s.append(f"    CNN_CH1 = {conv_channels};")
    s.append(f"    CNN_CH2 = {conv_channels};")
    s.append(f"    CNN_CH3 = {conv_channels if has_conv3 else 0};")

    for layer_name in layer_order:
        prefix = layer_name.upper()
        buf_w = f"{layer_name}_w" if layer_name not in ("policy", "value") else f"{layer_name}_w"
        buf_s = f"{layer_name}_s" if layer_name not in ("policy", "value") else None
        buf_b = f"{layer_name}_b"
        w_count = weight_data[f"{prefix}_W_COUNT"]
        s_count = weight_data[f"{prefix}_S_COUNT"]
        b_count = weight_data[f"{prefix}_B_COUNT"]

        if "out_channels" in model[layer_name]:
            # Conv layer: int4 weights + f32 scales + f32 bias
            s.append(f"    decode_int4({prefix}_W_ENC, {buf_w}, {w_count});")
            s.append(f"    decode_f32({prefix}_S_ENC, {buf_s}, {s_count});")
            s.append(f"    decode_f32({prefix}_B_ENC, {buf_b}, {b_count});")
        else:
            # Linear layer: int4 weights decoded to f32 (dequantize)
            # Need to dequantize: w_float[i] = w_int4[i] * scale[row]
            in_f = model[layer_name]["in_features"]
            out_f = model[layer_name]["out_features"]
            # Decode int4 into temp buffer, then dequantize to f32
            s.append(f"    {{ int8_t _tmp[{w_count}]; float _sc[{s_count}];")
            s.append(f"      decode_int4({prefix}_W_ENC, _tmp, {w_count});")
            s.append(f"      decode_f32({prefix}_S_ENC, _sc, {s_count});")
            s.append(f"      for (int o=0; o<{out_f}; o++)")
            s.append(f"        for (int i=0; i<{in_f}; i++)")
            s.append(f"          {buf_w}[o*{in_f}+i] = (float)_tmp[o*{in_f}+i] * _sc[o];")
            s.append(f"      decode_f32({prefix}_B_ENC, {buf_b}, {b_count}); }}")

    # Config values — only set vars that exist in the template
    for k, v in all_cfg.items():
        # Check if the variable is declared in the template
        if k not in template:
            continue
        if isinstance(v, float):
            s.append(f"    {k} = {v}f;")
        else:
            s.append(f"    {k} = {v};")

    s.append("}")

    weight_section = "\n".join(s)

    # Replace between markers
    start_marker = "// WEIGHTS_PLACEHOLDER_START"
    end_marker = "// WEIGHTS_PLACEHOLDER_END"
    start_pos = template.find(start_marker)
    end_pos = template.find(end_marker)

    if start_pos >= 0 and end_pos >= 0:
        template = (
            template[:start_pos]
            + weight_section
            + "\n"
            + template[end_pos + len(end_marker):]
        )
    else:
        raise ValueError("Template missing // WEIGHTS_PLACEHOLDER_START and // WEIGHTS_PLACEHOLDER_END markers")

    return template


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------

def verify_compile(path: Path) -> bool:
    """Check that the C file compiles with gcc."""
    try:
        proc = subprocess.run(
            ["gcc", "-std=c11", "-O2", "-lm", "-o", "/dev/null", str(path)],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if proc.returncode != 0:
            print(f"COMPILE CHECK FAILED:\n{proc.stderr}", file=sys.stderr)
            return False
        return True
    except FileNotFoundError:
        print("WARNING: gcc not found, skipping compile check", file=sys.stderr)
        return True
    except subprocess.TimeoutExpired:
        print("COMPILE CHECK TIMED OUT", file=sys.stderr)
        return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a C bot submission with embedded int4 neural network weights.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--weights", type=Path, required=True, help="Int4 weights JSON")
    parser.add_argument("--bot", type=Path, required=True, help="C bot template file")
    parser.add_argument("--output", type=Path, default=None, help="Output path")
    parser.add_argument("--no-verify", action="store_true", help="Skip compile check")
    # Config overrides
    parser.add_argument("--prior-mix", type=float, default=None)
    parser.add_argument("--leaf-mix", type=float, default=None)
    parser.add_argument("--value-scale", type=float, default=None)
    parser.add_argument("--prior-depth-limit", type=int, default=None)
    parser.add_argument("--leaf-depth-limit", type=int, default=None)
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    weights_path = args.weights if args.weights.is_absolute() else REPO_ROOT / args.weights
    bot_path = args.bot if args.bot.is_absolute() else REPO_ROOT / args.bot

    if not weights_path.exists():
        print(f"ERROR: Weights not found: {weights_path}", file=sys.stderr)
        sys.exit(1)
    if not bot_path.exists():
        print(f"ERROR: Bot template not found: {bot_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Loading int4 weights from {weights_path}")
    model = json.loads(weights_path.read_text(encoding="utf-8"))

    if model.get("quantization") != "int4_per_channel":
        print(f"WARNING: Expected int4_per_channel quantization, got {model.get('quantization')}")

    conv_channels = model["conv1"]["out_channels"]
    num_layers = 3 if model.get("conv3") is not None else 2
    desc = f"{conv_channels}ch/{num_layers}L"
    print(f"Model: {desc} (int4)")

    # Count params
    layer_names = ["conv1", "conv2"] + (["conv3"] if model.get("conv3") else []) + ["policy", "value"]
    total_int4 = sum(len(model[n]["weights_int4"]) for n in layer_names)
    total_scales = sum(len(model[n]["weight_scales"]) for n in layer_names)
    total_biases = sum(len(model[n]["bias"]) for n in layer_names)
    print(f"  {total_int4:,} int4 weights, {total_scales} scales, {total_biases} biases")

    # Encode weights
    print("Encoding weights...")
    weight_data = build_weight_data(model)
    int4_chars = weight_data["_total_int4_chars"]
    scale_chars = weight_data["_total_scale_chars"]
    bias_chars = weight_data["_total_bias_chars"]
    print(f"  Int4 weight chars: {int4_chars:,}")
    print(f"  Scale chars (f32): {scale_chars:,}")
    print(f"  Bias chars (f32):  {bias_chars:,}")
    print(f"  Total weight chars: {int4_chars + scale_chars + bias_chars:,}")

    # Build config overrides
    config_overrides: dict = {}
    for key in ("prior_mix", "leaf_mix", "value_scale", "prior_depth_limit", "leaf_depth_limit"):
        val = getattr(args, key, None)
        if val is not None:
            config_overrides[key] = val

    # Load and embed in template
    print(f"Loading C template from {bot_path}")
    template = bot_path.read_text(encoding="utf-8")
    combined = embed_in_template(template, weight_data, model, config_overrides)

    # Output path
    output_path = args.output
    if output_path is None:
        output_path = REPO_ROOT / "submission" / f"c_bot_{desc.replace('/', '_')}.c"
    elif not output_path.is_absolute():
        output_path = REPO_ROOT / output_path

    total_chars = len(combined)
    weight_total = int4_chars + scale_chars + bias_chars
    code_chars = total_chars - weight_total

    print()
    print("=" * 60)
    print("C SUBMISSION SUMMARY")
    print("=" * 60)
    print(f"  Model:          {desc} int4")
    print(f"  Int4 params:    {total_int4:,}")
    print(f"  Weight chars:   {weight_total:,}")
    print(f"  Code chars:     {code_chars:,}")
    print(f"  Total chars:    {total_chars:,}")
    print(f"  Char limit:     {CHAR_LIMIT:,}")
    print(f"  Remaining:      {CHAR_LIMIT - total_chars:,}")
    print(f"  Usage:          {total_chars / CHAR_LIMIT * 100:.1f}%")
    print("=" * 60)

    if total_chars > CHAR_LIMIT:
        overage = total_chars - CHAR_LIMIT
        print(f"\nFATAL: Submission exceeds 100K by {overage:,} chars!", file=sys.stderr)
        sys.exit(1)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(combined, encoding="utf-8")
    print(f"\nWrote submission to {output_path}")

    if not args.no_verify:
        print("Running compile check...")
        if verify_compile(output_path):
            print("Compile check PASSED")
        else:
            print("Compile check FAILED", file=sys.stderr)
            sys.exit(1)

    print("\nDone!")


if __name__ == "__main__":
    main()
