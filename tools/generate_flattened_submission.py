#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import json
import random
import re
import shutil
import struct
import subprocess
import tempfile
import textwrap
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--config",
        type=Path,
        default=REPO_ROOT / "rust/bot/configs/submission_current.json",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "submission/flattened_main.rs",
    )
    parser.add_argument(
        "--no-compile-check",
        action="store_true",
    )
    parser.add_argument(
        "--timing-probe",
        action="store_true",
        help="Inject timing measurement code (50 inferences on turn 0). "
        "Defaults output to submission/timing_probe.rs",
    )
    parser.add_argument(
        "--random-weights",
        type=str,
        default=None,
        metavar="SPEC",
        help='Generate random weights for a given architecture, e.g. "24ch/3L" or "28ch/2L". '
        "Use with --timing-probe to probe larger models without trained weights.",
    )
    return parser.parse_args()


def load_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def strip_test_modules(source: str) -> str:
    marker = "#[cfg(test)]"
    while True:
        start = source.find(marker)
        if start == -1:
            return source
        mod_start = source.find("mod tests", start)
        if mod_start == -1:
            raise RuntimeError(f"expected test module after {marker!r}")
        brace_start = source.find("{", mod_start)
        if brace_start == -1:
            raise RuntimeError("expected opening brace for test module")
        depth = 1
        index = brace_start + 1
        while depth > 0 and index < len(source):
            char = source[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
            index += 1
        if depth != 0:
            raise RuntimeError("unterminated test module")
        while index < len(source) and source[index] in "\r\n":
            index += 1
        source = source[:start] + source[index:]


def strip_serde(source: str) -> str:
    source = re.sub(r"^use serde::\{[^}]+\};\n", "", source, flags=re.MULTILINE)
    source = re.sub(r"^use serde::\w+;\n", "", source, flags=re.MULTILINE)
    source = source.replace("Serialize, Deserialize, ", "")
    source = source.replace("Deserialize, Serialize, ", "")
    source = source.replace(", Serialize, Deserialize", "")
    source = source.replace(", Deserialize, Serialize", "")
    source = source.replace("Serialize, Deserialize", "")
    source = source.replace("Deserialize, Serialize", "")
    source = source.replace(", Serialize", "")
    source = source.replace("Serialize, ", "")
    return source


def rewrite_engine_module(name: str, source: str) -> str:
    source = strip_serde(strip_test_modules(source))
    replacements = {
        "coord": {},
        "direction": {"use crate::Coord;": "use super::Coord;"},
        "map": {"use crate::{Coord, Direction};": "use super::{Coord, Direction};"},
        "state": {"use crate::{Coord, Direction, Grid, TileType};": "use super::{Coord, Direction, Grid, TileType};"},
    }
    for needle, repl in replacements.get(name, {}).items():
        source = source.replace(needle, repl)
    return source.strip() + "\n"


def _strip_function(source: str, func_name: str) -> str:
    """Remove a function definition (pub or private) from source by brace-matching."""
    for pat in (rf"pub fn {func_name}\b", rf"fn {func_name}\b"):
        start = re.search(pat, source)
        if start is None:
            continue
        fn_start = start.start()
        brace_start = source.find("{", start.end())
        if brace_start == -1:
            continue
        depth = 1
        index = brace_start + 1
        while depth > 0 and index < len(source):
            char = source[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
            index += 1
        if depth == 0:
            while index < len(source) and source[index] in "\r\n":
                index += 1
            source = source[:fn_start] + source[index:]
    return source


def strip_training_code(source: str) -> str:
    """Remove training-only structs and functions from features.rs.

    These reference types (OracleState, FinalResult, SearchStats, serde_json)
    that are not available in the flattened submission.
    """
    # Remove TrainingRow struct (pub struct TrainingRow { ... })
    for struct_name in ("TrainingRow", "TrainingMetadata"):
        pattern = rf"(?:#\[derive\([^\)]*\)\]\n)?(?:#\[serde[^\]]*\]\n)?pub struct {struct_name} \{{[^}}]*\}}"
        source = re.sub(pattern, "", source, flags=re.DOTALL)

    # Remove training-only and unused functions
    for func_name in ("encode_training_row", "stable_hash", "encode_position"):
        source = _strip_function(source, func_name)

    # Remove unused constants
    for const_name in ("GRID_CHANNELS", "VALUE_SCALE"):
        source = re.sub(
            rf"^(?:pub )?const {const_name}:.*;\n",
            "",
            source,
            flags=re.MULTILINE,
        )

    # Remove use crate::search::SearchStats (may already be rewritten to super::)
    source = re.sub(r"^use super::search::SearchStats;\n", "", source, flags=re.MULTILINE)

    # Clean up excessive blank lines
    source = re.sub(r"\n{3,}", "\n\n", source)
    return source


def strip_unused_engine_code(source: str) -> str:
    """Remove unused engine functions from map.rs and state.rs source.

    These functions are not called by the bot and waste space.
    """
    for func_name in (
        "detect_air_pockets",
        "detect_spawn_islands",
        "detect_regions",
        "detect_lowest_island",
        "get_free_above",
        "sorted_unique_apples",
        "neighbours",
        "encode_player_view",
    ):
        source = _strip_function(source, func_name)

    # Remove the ADJACENCY constant used only by the stripped functions
    source = re.sub(
        r"^[ \t]*(?:pub )?const ADJACENCY:.*;\n",
        "",
        source,
        flags=re.MULTILINE,
    )

    # Remove VisibilityState struct (unused in bot code)
    source = re.sub(
        r"(?:#\[derive\([^\)]*\)\]\n)?pub struct VisibilityState \{[^}]*\}\n",
        "",
        source,
        flags=re.DOTALL,
    )

    # Clean up excessive blank lines
    source = re.sub(r"\n{3,}", "\n\n", source)
    return source


def rewrite_bot_module(name: str, source: str) -> str:
    source = strip_serde(strip_test_modules(source))
    replacements = {
        "input": {
            "use snakebot_engine::{BirdState, Coord, GameState, Grid, TileType};": "use crate::engine::{BirdState, Coord, GameState, Grid, TileType};",
        },
        "eval": {
            "use snakebot_engine::{Coord, GameState};": "use crate::engine::{Coord, GameState, TileType};",
            "use crate::config::EvalWeights;": "use super::config::EvalWeights;",
            "snakebot_engine::TileType::Wall": "TileType::Wall",
        },
        "features": {
            "use snakebot_engine::{BirdCommand, Coord, Direction, FinalResult, GameState, OracleState, PlayerAction, TileType};": "use crate::engine::{BirdCommand, Coord, Direction, GameState, PlayerAction, TileType};",
            "use crate::search::SearchStats;": "use super::search::SearchStats;",
        },
        "hybrid": {
            "use snakebot_engine::{GameState, PlayerAction};": "use crate::engine::{GameState, PlayerAction};",
            "use crate::config::{BotConfig, HybridConfig};": "use super::config::{BotConfig, HybridConfig};",
            "use crate::features::{encode_hybrid_position, policy_targets_for_action, HYBRID_GRID_CHANNELS, MAX_BIRDS_PER_PLAYER, POLICY_ACTIONS_PER_BIRD, SCALAR_FEATURES};": "use super::features::{encode_hybrid_position, policy_targets_for_action, HYBRID_GRID_CHANNELS, MAX_BIRDS_PER_PLAYER, POLICY_ACTIONS_PER_BIRD, SCALAR_FEATURES};",
        },
        "search": {
            "use snakebot_engine::{BirdCommand, Direction, GameState, PlayerAction};": "use crate::engine::{BirdCommand, Direction, GameState, PlayerAction};",
            "use crate::config::BotConfig;": "use super::config::BotConfig;",
            "use crate::eval::evaluate;": "use super::eval::evaluate;",
            "use crate::hybrid::{leaf_bonus, predict, HybridPrediction};": "use super::hybrid::{leaf_bonus, predict, HybridPrediction};",
        },
    }
    for needle, repl in replacements.get(name, {}).items():
        source = source.replace(needle, repl)
    return source.strip() + "\n"


def wrap_module(name: str, body: str, *, indent: str = "    ") -> str:
    return f"pub mod {name} {{\n{textwrap.indent(body.rstrip(), indent)}\n}}\n"


def build_config_module(
    config_path: Path,
    *,
    embedded_hybrid: bool = False,
    payload_override: dict | None = None,
) -> str:
    payload = payload_override if payload_override is not None else json.loads(load_text(config_path))
    eval_payload = payload["eval"]
    search_payload = payload["search"]
    hybrid_payload = payload.get("hybrid")
    name_literal = json.dumps(payload["name"])
    hybrid_literal = "None"
    if hybrid_payload:
        if embedded_hybrid:
            weights_path_literal = 'Some("__embedded__".to_owned())'
        else:
            weights_path = json.dumps(hybrid_payload.get("weights_path"))
            weights_path_literal = f"{weights_path}.map(str::to_owned)"
        prior_depth = hybrid_payload.get("prior_depth_limit", "usize::MAX")
        leaf_depth = hybrid_payload.get("leaf_depth_limit", "usize::MAX")
        prior_depth_literal = "usize::MAX" if prior_depth == "usize::MAX" else str(prior_depth)
        leaf_depth_literal = "usize::MAX" if leaf_depth == "usize::MAX" else str(leaf_depth)
        hybrid_literal = textwrap.dedent(
            f"""
            Some(HybridConfig {{
                weights_path: {weights_path_literal},
                prior_mix: {hybrid_payload.get("prior_mix", 0.0)},
                leaf_mix: {hybrid_payload.get("leaf_mix", 0.0)},
                value_scale: {hybrid_payload.get("value_scale", 48.0)},
                prior_depth_limit: {prior_depth_literal},
                leaf_depth_limit: {leaf_depth_literal},
            }})
            """
        ).strip()
    return textwrap.dedent(
        f"""
        #[derive(Clone, Copy, Debug, PartialEq)]
        pub struct EvalWeights {{
            pub body: f64,
            pub loss: f64,
            pub mobility: f64,
            pub apple: f64,
            pub stability: f64,
            pub breakpoint: f64,
            pub fragile_attack: f64,
            pub terminal: f64,
        }}

        impl Default for EvalWeights {{
            fn default() -> Self {{
                Self {{
                    body: {eval_payload["body"]},
                    loss: {eval_payload["loss"]},
                    mobility: {eval_payload["mobility"]},
                    apple: {eval_payload["apple"]},
                    stability: {eval_payload["stability"]},
                    breakpoint: {eval_payload["breakpoint"]},
                    fragile_attack: {eval_payload["fragile_attack"]},
                    terminal: {eval_payload["terminal"]},
                }}
            }}
        }}

        #[derive(Clone, Copy, Debug, PartialEq)]
        pub struct SearchConfig {{
            pub first_turn_ms: u64,
            pub later_turn_ms: u64,
            pub deepen_top_my: usize,
            pub deepen_top_opp: usize,
            pub deepen_child_my: usize,
            pub deepen_child_opp: usize,
            pub extra_nodes_after_root: u64,
        }}

        impl Default for SearchConfig {{
            fn default() -> Self {{
                Self {{
                    first_turn_ms: {search_payload["first_turn_ms"]},
                    later_turn_ms: {search_payload["later_turn_ms"]},
                    deepen_top_my: {search_payload["deepen_top_my"]},
                    deepen_top_opp: {search_payload["deepen_top_opp"]},
                    deepen_child_my: {search_payload["deepen_child_my"]},
                    deepen_child_opp: {search_payload["deepen_child_opp"]},
                    extra_nodes_after_root: {search_payload["extra_nodes_after_root"]},
                }}
            }}
        }}

        #[derive(Clone, Debug, PartialEq)]
        pub struct HybridConfig {{
            pub weights_path: Option<String>,
            pub prior_mix: f64,
            pub leaf_mix: f64,
            pub value_scale: f64,
            pub prior_depth_limit: usize,
            pub leaf_depth_limit: usize,
        }}

        impl Default for HybridConfig {{
            fn default() -> Self {{
                Self {{
                    weights_path: None,
                    prior_mix: 0.0,
                    leaf_mix: 0.0,
                    value_scale: 48.0,
                    prior_depth_limit: usize::MAX,
                    leaf_depth_limit: usize::MAX,
                }}
            }}
        }}

        impl HybridConfig {{
            pub fn is_enabled(&self) -> bool {{
                self.weights_path.is_some() && (self.prior_mix != 0.0 || self.leaf_mix != 0.0)
            }}
        }}

        #[derive(Clone, Debug, PartialEq)]
        pub struct BotConfig {{
            pub name: String,
            pub eval: EvalWeights,
            pub search: SearchConfig,
            pub hybrid: Option<HybridConfig>,
        }}

        impl Default for BotConfig {{
            fn default() -> Self {{
                Self::embedded()
            }}
        }}

        impl BotConfig {{
            pub fn embedded() -> Self {{
                Self {{
                    name: {name_literal}.to_owned(),
                    eval: EvalWeights::default(),
                    search: SearchConfig::default(),
                    hybrid: {hybrid_literal},
                }}
            }}
        }}
        """
    ).strip() + "\n"


def build_minimal_features_module() -> str:
    return "// search-only flattened submission does not need training features\n"


def build_disabled_hybrid_module() -> str:
    return textwrap.dedent(
        """
        use crate::engine::{GameState, PlayerAction};
        use super::config::{BotConfig, HybridConfig};

        #[derive(Clone, Debug)]
        pub struct HybridPrediction;

        impl HybridPrediction {
            pub fn action_prior(&self, _state: &GameState, _owner: usize, _action: &PlayerAction) -> f64 {
                0.0
            }
        }

        pub fn predict(_state: &GameState, _owner: usize, _config: &BotConfig) -> Option<HybridPrediction> {
            None
        }

        pub fn leaf_bonus(_prediction: &HybridPrediction, _hybrid: &HybridConfig) -> f64 {
            0.0
        }
        """
    ).strip() + "\n"


def _encode_weights_base64(model: dict) -> str:
    """Serialize all weight/bias floats into a single base64 string.

    Order: conv1.weights, conv1.bias, conv2.weights, conv2.bias,
           conv3.weights, conv3.bias (if present),
           policy.weights, policy.bias, value.weights, value.bias
    """
    buf = b""
    layer_order = ["conv1", "conv2"]
    if model.get("conv3") is not None:
        layer_order.append("conv3")
    layer_order.extend(["policy", "value"])
    for layer_name in layer_order:
        layer = model[layer_name]
        for array_name in ("weights", "bias"):
            values = layer[array_name]
            buf += struct.pack(f"<{len(values)}f", *values)
    return base64.b64encode(buf).decode("ascii")


def _collect_weight_bytes(model: dict, *, dtype: str = "f32") -> bytes:
    """Pack all weight/bias floats into a byte buffer.

    Order: conv1.w, conv1.b, conv2.w, conv2.b, [conv3.w, conv3.b], policy.w, policy.b, value.w, value.b
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
        buf += b'\x00'
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
    return ''.join(chars)


def _encode_weights_unicode(model: dict) -> str:
    """Encode weights as f32 Unicode BMP (2 chars per f32, ~2.6x denser than base64)."""
    return _bytes_to_unicode(_collect_weight_bytes(model, dtype="f32"))


def _encode_weights_f16_unicode(model: dict) -> str:
    """Encode weights as f16 Unicode BMP (1 char per weight, ~5.3x denser than base64)."""
    return _bytes_to_unicode(_collect_weight_bytes(model, dtype="f16"))


def _rust_unicode_literal(s: str) -> str:
    """Convert a Unicode string to a safe Rust string literal body.

    Escapes control characters, backslash, and double quote using \\u{XXXX}.
    All other BMP codepoints are written as raw UTF-8.
    """
    parts = []
    for ch in s:
        cp = ord(ch)
        if cp < 0x20 or cp == 0x5C or cp == 0x22 or cp == 0x7F:
            parts.append(f'\\u{{{cp:04x}}}')
        else:
            parts.append(ch)
    return ''.join(parts)


def _compute_layer_sizes(model: dict) -> list[tuple[str, str, int]]:
    """Return list of (layer_name, array_name, count) for weight decoding."""
    sizes: list[tuple[str, str, int]] = []
    layer_order = ["conv1", "conv2"]
    if model.get("conv3") is not None:
        layer_order.append("conv3")
    layer_order.extend(["policy", "value"])
    for layer_name in layer_order:
        layer = model[layer_name]
        for array_name in ("weights", "bias"):
            sizes.append((layer_name, array_name, len(layer[array_name])))
    return sizes


def build_embedded_hybrid_module(
    weights_path: Path,
    *,
    timing_probe: bool = False,
    model_desc: str = "",
) -> str:
    """Build a hybrid module with neural network weights embedded as Unicode BMP."""
    raw = weights_path.read_text(encoding="utf-8")
    model = json.loads(raw)

    unicode_string = _encode_weights_f16_unicode(model)
    unicode_literal = _rust_unicode_literal(unicode_string)
    sizes = _compute_layer_sizes(model)
    has_conv3 = model.get("conv3") is not None

    # Build the size constants for decoding
    size_entries = []
    for layer_name, array_name, count in sizes:
        const_name = f"{layer_name.upper()}_{array_name[0].upper()}_LEN"
        size_entries.append(f"const {const_name}: usize = {count};")
    size_consts = "\n".join(size_entries)

    # Build the take calls for get_embedded_model
    take_lines = []
    for layer_name, array_name, _ in sizes:
        const_name = f"{layer_name.upper()}_{array_name[0].upper()}_LEN"
        var_name = f"{layer_name}_{array_name[0]}"
        take_lines.append(f"let {var_name} = take(&all, &mut off, {const_name});")
    take_block = "\n".join(f"    {line}" for line in take_lines)

    # Build layer constructors using the decoded Vecs (leaked to get &'static)
    def conv_constructor(name: str) -> str:
        layer = model[name]
        w_var = f"{name}_w"
        b_var = f"{name}_b"
        return (
            f"ConvLayer {{\n"
            f"    out_channels: {layer['out_channels']},\n"
            f"    in_channels: {layer['in_channels']},\n"
            f"    kernel_size: {layer['kernel_size']},\n"
            f"    weights: Vec::leak({w_var}),\n"
            f"    bias: Vec::leak({b_var}),\n"
            f"}}"
        )

    def linear_constructor(name: str) -> str:
        layer = model[name]
        w_var = f"{name}_w"
        b_var = f"{name}_b"
        return (
            f"LinearLayer {{\n"
            f"    out_features: {layer['out_features']},\n"
            f"    in_features: {layer['in_features']},\n"
            f"    weights: Vec::leak({w_var}),\n"
            f"    bias: Vec::leak({b_var}),\n"
            f"}}"
        )

    conv3_field = f"conv3: Some({conv_constructor('conv3')})," if has_conv3 else "conv3: None,"

    if timing_probe:
        escaped_desc = model_desc.replace('"', '\\"')
        timing_probe_fn = textwrap.dedent(f"""
        pub fn timing_probe(state: &GameState, owner: usize, n: usize) -> String {{
            let model = get_embedded_model();
            let encoded = encode_hybrid_position(state, owner);
            let start = std::time::Instant::now();
            for _ in 0..n {{
                let _ = model.forward(&encoded.grid, &encoded.scalars);
            }}
            let elapsed = start.elapsed();
            format!("TIMING {escaped_desc}: {{}} inferences in {{:.1}}ms ({{:.2}}ms/call)",
                    n, elapsed.as_secs_f64() * 1000.0, elapsed.as_secs_f64() * 1000.0 / n as f64)
        }}
        """).rstrip() + "\n"
    else:
        timing_probe_fn = ""

    return textwrap.dedent(f"""
        use std::sync::OnceLock;

        use crate::engine::{{GameState, PlayerAction}};
        use super::config::{{BotConfig, HybridConfig}};
        use super::features::{{encode_hybrid_position, policy_targets_for_action, HYBRID_GRID_CHANNELS, POLICY_ACTIONS_PER_BIRD, SCALAR_FEATURES}};

        #[derive(Clone, Debug)]
        pub struct HybridPrediction {{
            pub policy_logits: Vec<f32>,
            pub value: f32,
        }}

        impl HybridPrediction {{
            pub fn action_prior(&self, state: &GameState, owner: usize, action: &PlayerAction) -> f64 {{
                let targets = policy_targets_for_action(state, owner, action);
                let mut total = 0.0_f64;
                for (slot_idx, target) in targets.into_iter().enumerate() {{
                    if target < 0 {{
                        continue;
                    }}
                    let index = slot_idx * POLICY_ACTIONS_PER_BIRD + target as usize;
                    if let Some(logit) = self.policy_logits.get(index) {{
                        total += f64::from(*logit);
                    }}
                }}
                total
            }}
        }}

        struct ConvLayer {{
            out_channels: usize,
            in_channels: usize,
            kernel_size: usize,
            weights: &'static [f32],
            bias: &'static [f32],
        }}

        struct LinearLayer {{
            out_features: usize,
            in_features: usize,
            weights: &'static [f32],
            bias: &'static [f32],
        }}

        struct TinyHybridWeights {{
            input_channels: usize,
            scalar_features: usize,
            conv1: ConvLayer,
            conv2: ConvLayer,
            conv3: Option<ConvLayer>,
            policy: LinearLayer,
            value: LinearLayer,
        }}

        {size_consts}

        static WEIGHTS_F16: &str = "{unicode_literal}";

        fn f16_to_f32(bits: u16) -> f32 {{
            let s = ((bits >> 15) & 1) as u32;
            let e = ((bits >> 10) & 0x1F) as u32;
            let m = (bits & 0x3FF) as u32;
            if e == 0 {{
                if m == 0 {{ return f32::from_bits(s << 31); }}
                let mut mm = m;
                let mut ee = 1u32;
                while mm & 0x400 == 0 {{ mm <<= 1; ee += 1; }}
                return f32::from_bits((s << 31) | ((127 - 15 + 1 - ee) << 23) | ((mm & 0x3FF) << 13));
            }}
            if e == 31 {{ return f32::from_bits((s << 31) | (0xFF << 23) | (m << 13)); }}
            f32::from_bits((s << 31) | ((e - 15 + 127) << 23) | (m << 13))
        }}

        fn decode_f16_weights(input: &str) -> Vec<f32> {{
            let mut out = Vec::with_capacity(input.len());
            let mut it = input.chars();
            while let Some(ch) = it.next() {{
                let v = ch as u32;
                let u16v = if v == 0xFFFF {{
                    let off = it.next().unwrap_or('\\x00') as u32;
                    if off == 0x0800 {{ 0xFFFFu16 }} else {{ (0xD800 + off) as u16 }}
                }} else {{
                    v as u16
                }};
                out.push(f16_to_f32(u16v));
            }}
            out
        }}

        fn get_embedded_model() -> &'static TinyHybridWeights {{
            static MODEL: OnceLock<TinyHybridWeights> = OnceLock::new();
            MODEL.get_or_init(|| {{
                let all = decode_f16_weights(WEIGHTS_F16);
                let mut off: usize = 0;
                let take = |all: &[f32], off: &mut usize, n: usize| -> Vec<f32> {{
                    let s = all[*off..*off+n].to_vec(); *off += n; s
                }};
        {take_block}
                TinyHybridWeights {{
                    input_channels: {model['input_channels']},
                    scalar_features: {model['scalar_features']},
                    conv1: {conv_constructor('conv1')},
                    conv2: {conv_constructor('conv2')},
                    {conv3_field}
                    policy: {linear_constructor('policy')},
                    value: {linear_constructor('value')},
                }}
            }})
        }}

        pub fn predict(state: &GameState, owner: usize, config: &BotConfig) -> Option<HybridPrediction> {{
            let hybrid = config.hybrid.as_ref()?;
            if !hybrid.is_enabled() {{
                return None;
            }}
            let model = get_embedded_model();
            let encoded = encode_hybrid_position(state, owner);
            if encoded.grid.len() != model.input_channels || encoded.scalars.len() != model.scalar_features {{
                return None;
            }}
            Some(model.forward(&encoded.grid, &encoded.scalars))
        }}

        pub fn leaf_bonus(prediction: &HybridPrediction, hybrid: &HybridConfig) -> f64 {{
            f64::from(prediction.value) * hybrid.value_scale
        }}
{timing_probe_fn}
        impl TinyHybridWeights {{
            fn forward(&self, grid: &[Vec<Vec<f32>>], scalars: &[f32]) -> HybridPrediction {{
                let height = grid.first().map(|channel| channel.len()).unwrap_or(0);
                let width = grid
                    .first()
                    .and_then(|channel| channel.first())
                    .map(|row| row.len())
                    .unwrap_or(0);
                let mut flat_input = Vec::with_capacity(grid.len() * height * width);
                for channel in grid {{
                    for row in channel {{
                        flat_input.extend_from_slice(row);
                    }}
                }}
                let conv1_out = self.conv1.forward_flat(&flat_input, height, width);
                let conv2_out = self.conv2.forward_flat(&conv1_out, height, width);
                let (last_out, last_channels) = match self.conv3 {{
                    Some(ref conv3) => (conv3.forward_flat(&conv2_out, height, width), conv3.out_channels),
                    None => (conv2_out, self.conv2.out_channels),
                }};
                let pooled = global_average_pool_flat(&last_out, last_channels, height, width);
                let mut features = pooled;
                features.extend_from_slice(scalars);
                let policy_logits = self.policy.forward(&features);
                let value = self
                    .value
                    .forward(&features)
                    .first()
                    .copied()
                    .unwrap_or_default()
                    .tanh();
                HybridPrediction {{
                    policy_logits,
                    value,
                }}
            }}
        }}

        impl ConvLayer {{
            fn forward_flat(
                &self,
                input: &[f32],
                height: usize,
                width: usize,
            ) -> Vec<f32> {{
                let hw = height * width;
                let mut output = vec![0.0_f32; self.out_channels * hw];

                if self.kernel_size == 1 {{
                    for oc in 0..self.out_channels {{
                        let out_base = oc * hw;
                        let bias_val = self.bias[oc];
                        for i in 0..hw {{
                            let mut acc = bias_val;
                            for ic in 0..self.in_channels {{
                                acc += input[ic * hw + i] * self.weights[oc * self.in_channels + ic];
                            }}
                            output[out_base + i] = acc.max(0.0);
                        }}
                    }}
                }} else if self.kernel_size == 3 {{
                    for oc in 0..self.out_channels {{
                        let bias_val = self.bias[oc];
                        let out_base = oc * hw;
                        for i in 0..hw {{
                            output[out_base + i] = bias_val;
                        }}
                    }}
                    for oc in 0..self.out_channels {{
                        let out_base = oc * hw;
                        for ic in 0..self.in_channels {{
                            let in_base = ic * hw;
                            let w_base = (oc * self.in_channels + ic) * 9;
                            let w = &self.weights[w_base..w_base + 9];
                            for y in 1..height - 1 {{
                                let row_off = y * width;
                                for x in 1..width - 1 {{
                                    let out_idx = out_base + row_off + x;
                                    let mut acc = 0.0_f32;
                                    acc += input[in_base + (y - 1) * width + (x - 1)] * w[0];
                                    acc += input[in_base + (y - 1) * width + x] * w[1];
                                    acc += input[in_base + (y - 1) * width + (x + 1)] * w[2];
                                    acc += input[in_base + y * width + (x - 1)] * w[3];
                                    acc += input[in_base + y * width + x] * w[4];
                                    acc += input[in_base + y * width + (x + 1)] * w[5];
                                    acc += input[in_base + (y + 1) * width + (x - 1)] * w[6];
                                    acc += input[in_base + (y + 1) * width + x] * w[7];
                                    acc += input[in_base + (y + 1) * width + (x + 1)] * w[8];
                                    output[out_idx] += acc;
                                }}
                            }}
                            for y in [0, height - 1] {{
                                for x in 0..width {{
                                    let out_idx = out_base + y * width + x;
                                    for ky in 0..3usize {{
                                        for kx in 0..3usize {{
                                            let iy = y as isize + ky as isize - 1;
                                            let ix = x as isize + kx as isize - 1;
                                            if iy >= 0
                                                && ix >= 0
                                                && iy < height as isize
                                                && ix < width as isize
                                            {{
                                                output[out_idx] += input
                                                    [in_base + iy as usize * width + ix as usize]
                                                    * w[ky * 3 + kx];
                                            }}
                                        }}
                                    }}
                                }}
                            }}
                            for y in 1..height - 1 {{
                                for x in [0, width - 1] {{
                                    let out_idx = out_base + y * width + x;
                                    for ky in 0..3usize {{
                                        for kx in 0..3usize {{
                                            let iy = y as isize + ky as isize - 1;
                                            let ix = x as isize + kx as isize - 1;
                                            if iy >= 0
                                                && ix >= 0
                                                && iy < height as isize
                                                && ix < width as isize
                                            {{
                                                output[out_idx] += input
                                                    [in_base + iy as usize * width + ix as usize]
                                                    * w[ky * 3 + kx];
                                            }}
                                        }}
                                    }}
                                }}
                            }}
                        }}
                    }}
                    for v in output.iter_mut() {{
                        *v = v.max(0.0);
                    }}
                }} else {{
                    let pad = (self.kernel_size / 2) as isize;
                    for oc in 0..self.out_channels {{
                        for y in 0..height {{
                            for x in 0..width {{
                                let mut acc = self.bias[oc];
                                for ic in 0..self.in_channels {{
                                    for ky in 0..self.kernel_size {{
                                        for kx in 0..self.kernel_size {{
                                            let iy = y as isize + ky as isize - pad;
                                            let ix = x as isize + kx as isize - pad;
                                            if iy < 0
                                                || ix < 0
                                                || iy >= height as isize
                                                || ix >= width as isize
                                            {{
                                                continue;
                                            }}
                                            let w_idx =
                                                ((oc * self.in_channels + ic) * self.kernel_size + ky)
                                                    * self.kernel_size
                                                    + kx;
                                            acc += input[ic * hw + iy as usize * width + ix as usize]
                                                * self.weights[w_idx];
                                        }}
                                    }}
                                }}
                                output[oc * hw + y * width + x] = acc.max(0.0);
                            }}
                        }}
                    }}
                }}

                output
            }}
        }}

        impl LinearLayer {{
            fn forward(&self, input: &[f32]) -> Vec<f32> {{
                let mut output = vec![0.0_f32; self.out_features];
                for (out_index, slot) in output.iter_mut().enumerate() {{
                    let mut acc = self.bias[out_index];
                    let base = out_index * self.in_features;
                    for in_index in 0..self.in_features {{
                        acc += input[in_index] * self.weights[base + in_index];
                    }}
                    *slot = acc;
                }}
                output
            }}
        }}

        fn global_average_pool_flat(input: &[f32], channels: usize, height: usize, width: usize) -> Vec<f32> {{
            let hw = height * width;
            let norm = hw.max(1) as f32;
            (0..channels)
                .map(|c| {{
                    let start = c * hw;
                    input[start..start + hw].iter().sum::<f32>() / norm
                }})
                .collect()
        }}
    """).strip() + "\n"


def build_main_module(*, timing_probe: bool = False) -> str:
    if timing_probe:
        timing_block = (
            "                if turn_count == 0 {\n"
            "                    let msg = bot::hybrid::timing_probe(&state, self.io.player_index, 50);\n"
            '                    eprintln!("{msg}");\n'
            "                }\n"
            "                turn_count += 1;\n"
        )
        turn_counter_init = "\n            let mut turn_count: u32 = 0;"
    else:
        timing_block = ""
        turn_counter_init = ""

    return textwrap.dedent(
        f"""
        use std::io::{{self, BufRead, BufReader, Write}};

        use bot::config::BotConfig;
        use bot::input::{{BotIo, FrameObservation}};
        use bot::search::{{choose_action, live_budget_for_turn, render_action}};
        use engine::{{GameState, PlayerAction}};

        fn main() -> io::Result<()> {{
            let stdin = io::stdin();
            let stdout = io::stdout();
            let mut reader = BufReader::new(stdin.lock());
            let mut writer = io::BufWriter::new(stdout.lock());
            let bot = Bot::new(BotIo::read(&mut reader)?);
            bot.run(&mut reader, &mut writer)
        }}

        struct Bot {{
            config: BotConfig,
            io: BotIo,
            state: Option<GameState>,
            last_my_action: PlayerAction,
        }}

        impl Bot {{
            fn new(io: BotIo) -> Self {{
                Self {{
                    config: BotConfig::embedded(),
                    io,
                    state: None,
                    last_my_action: PlayerAction::default(),
                }}
            }}

            fn run(mut self, reader: &mut impl BufRead, writer: &mut impl Write) -> io::Result<()> {{
                let debug_stats = std::env::var_os("SNAKEBOT_DEBUG_STATS").is_some();{turn_counter_init}
                while let Some(frame) = self.io.read_frame(reader)? {{
                    let state = self.reconcile_state(&frame);
{timing_block}\
                    let budget = live_budget_for_turn(&self.config, state.turn);
                    let outcome = choose_action(&state, self.io.player_index, &self.config, budget);
                    if debug_stats {{
                        eprintln!(
                            "turn={{}} elapsed_ms={{}} root_pairs={{}} extra_nodes={{}} cutoffs={{}}",
                            state.turn,
                            outcome.stats.elapsed_ms,
                            outcome.stats.root_pairs,
                            outcome.stats.extra_nodes,
                            outcome.stats.cutoffs
                        );
                    }}
                    let command = render_action(&outcome.action);
                    writeln!(writer, "{{command}}")?;
                    writer.flush()?;
                    self.last_my_action = outcome.action;
                    self.state = Some(state);
                }}
                Ok(())
            }}

            fn reconcile_state(&self, frame: &FrameObservation) -> GameState {{
                let observed_signature = self.io.observation_signature(frame);
                let fallback = if let Some(previous) = &self.state {{
                    self.io
                        .build_visible_state(frame, previous.losses, previous.turn + 1)
                }} else {{
                    self.io.build_visible_state(frame, [0, 0], 0)
                }};

                let Some(previous) = &self.state else {{
                    return fallback;
                }};

                let opponent = 1 - self.io.player_index;
                for opp_action in previous.legal_joint_actions(opponent) {{
                    let mut simulated = previous.clone();
                    if self.io.player_index == 0 {{
                        simulated.step(&self.last_my_action, &opp_action);
                    }} else {{
                        simulated.step(&opp_action, &self.last_my_action);
                    }}
                    if self.io.visible_signature(&simulated) == observed_signature {{
                        return simulated;
                    }}
                }}

                if std::env::var_os("SNAKEBOT_STRICT_RECONCILE").is_some() {{
                    panic!("reconcile_state fell back to visible reconstruction");
                }}

                fallback
            }}
        }}
        """
    ).strip() + "\n"


def compile_check(path: Path) -> None:
    target = path.with_suffix("")
    subprocess.run(
        [
            "rustc",
            "--edition=2021",
            str(path),
            "-O",
            "-o",
            str(target),
        ],
        cwd=REPO_ROOT,
        check=True,
    )


def format_output(path: Path) -> None:
    rustfmt = shutil.which("rustfmt")
    if rustfmt is None:
        return
    subprocess.run(
        [
            rustfmt,
            "--edition=2021",
            str(path),
        ],
        cwd=REPO_ROOT,
        check=True,
    )


def compact_output(path: Path) -> int:
    """Strip comments, blank lines, reduce indentation to fit CodinGame 100K limit."""
    content = path.read_text(encoding="utf-8")
    lines = content.split("\n")
    out: list[str] = []
    for line in lines:
        stripped = line.rstrip()
        s = stripped.lstrip()
        # Skip pure comment lines (short ones -- long ones may be data like base64)
        if s.startswith("// ") and len(s) < 200:
            continue
        # Skip blank lines
        if stripped == "":
            continue
        # Reduce indentation: 4-space levels -> 1-space levels
        leading = len(stripped) - len(s)
        indent_level = leading // 4
        remainder = leading % 4
        out.append(" " * indent_level + " " * remainder + s)
    result = "\n".join(out) + "\n"
    path.write_text(result, encoding="utf-8")
    return len(result)


def _model_desc_from_weights(weights_path: Path) -> str:
    """Derive a short model description like '12ch/3L' from the weights JSON."""
    model = json.loads(weights_path.read_text(encoding="utf-8"))
    conv_channels = model.get("conv1", {}).get("out_channels", "?")
    num_layers = 3 if model.get("conv3") is not None else 2
    return f"{conv_channels}ch/{num_layers}L"


def _parse_random_weights_spec(spec: str) -> tuple[int, int]:
    """Parse a spec like '24ch/3L' into (channels, num_conv_layers)."""
    import re as _re

    m = _re.match(r"(\d+)ch/(\d+)L", spec)
    if not m:
        raise SystemExit(
            f"Invalid --random-weights spec {spec!r}, expected format like '24ch/3L'"
        )
    return int(m.group(1)), int(m.group(2))


def _generate_random_weights(spec: str) -> tuple[Path, str]:
    """Generate a temporary weights JSON with random values for the given architecture.

    Returns (temp_path, model_desc) where temp_path is a temporary file containing
    the JSON weights and model_desc is like '24ch/3L'.
    """
    channels, num_layers = _parse_random_weights_spec(spec)
    if num_layers not in (2, 3):
        raise SystemExit(f"Only 2 or 3 conv layers supported, got {num_layers}")

    INPUT_CHANNELS = 19  # HYBRID_GRID_CHANNELS
    SCALAR_FEATURES = 6
    MAX_BIRDS = 4  # MAX_BIRDS_PER_PLAYER
    POLICY_ACTIONS = 5  # POLICY_ACTIONS_PER_BIRD
    policy_out = MAX_BIRDS * POLICY_ACTIONS

    rng = random.Random(42)  # deterministic for reproducibility

    def rand_floats(n: int) -> list[float]:
        return [rng.uniform(-0.1, 0.1) for _ in range(n)]

    model: dict = {
        "version": 2,
        "input_channels": INPUT_CHANNELS,
        "scalar_features": SCALAR_FEATURES,
        "board_height": 23,
        "board_width": 42,
        "conv1": {
            "out_channels": channels,
            "in_channels": INPUT_CHANNELS,
            "kernel_size": 3,
            "weights": rand_floats(channels * INPUT_CHANNELS * 9),
            "bias": rand_floats(channels),
        },
        "conv2": {
            "out_channels": channels,
            "in_channels": channels,
            "kernel_size": 3,
            "weights": rand_floats(channels * channels * 9),
            "bias": rand_floats(channels),
        },
    }
    if num_layers >= 3:
        model["conv3"] = {
            "out_channels": channels,
            "in_channels": channels,
            "kernel_size": 3,
            "weights": rand_floats(channels * channels * 9),
            "bias": rand_floats(channels),
        }

    linear_in = channels + SCALAR_FEATURES
    model["policy"] = {
        "out_features": policy_out,
        "in_features": linear_in,
        "weights": rand_floats(policy_out * linear_in),
        "bias": rand_floats(policy_out),
    }
    model["value"] = {
        "out_features": 1,
        "in_features": linear_in,
        "weights": rand_floats(linear_in),
        "bias": rand_floats(1),
    }

    desc = f"{channels}ch/{num_layers}L"

    tmp = tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".json",
        prefix=f"snakebot-random-{desc.replace('/', '-')}-",
        delete=False,
    )
    json.dump(model, tmp)
    tmp.close()
    return Path(tmp.name), desc


def main() -> None:
    args = parse_args()
    config_path = args.config.resolve()
    timing_probe = args.timing_probe
    random_weights_spec = args.random_weights

    # Override output default when --timing-probe is used and --output was not explicitly set
    if timing_probe and args.output == REPO_ROOT / "submission/flattened_main.rs":
        output_path = (REPO_ROOT / "submission/timing_probe.rs").resolve()
    else:
        output_path = args.output.resolve()

    config_payload = json.loads(load_text(config_path))
    hybrid_enabled = bool(config_payload.get("hybrid"))

    # --random-weights implies hybrid and overrides the weights path
    random_weights_path: Path | None = None
    random_weights_desc: str = ""
    if random_weights_spec:
        if not timing_probe:
            raise SystemExit("--random-weights requires --timing-probe")
        random_weights_path, random_weights_desc = _generate_random_weights(
            random_weights_spec
        )
        # Force hybrid enabled with a synthetic config entry
        if not hybrid_enabled:
            config_payload["hybrid"] = {
                "weights_path": str(random_weights_path),
                "prior_mix": 0.25,
                "leaf_mix": 0.5,
                "value_scale": 48.0,
            }
            hybrid_enabled = True
        else:
            config_payload["hybrid"]["weights_path"] = str(random_weights_path)

    if timing_probe and not hybrid_enabled:
        raise SystemExit("--timing-probe requires a hybrid config with embedded weights")

    mod_indent = " " if hybrid_enabled else "    "

    engine_dir = REPO_ROOT / "rust/engine/src"
    bot_dir = REPO_ROOT / "rust/bot/src"

    coord = rewrite_engine_module("coord", load_text(engine_dir / "coord.rs"))
    direction = rewrite_engine_module("direction", load_text(engine_dir / "direction.rs"))
    map_mod = rewrite_engine_module("map", load_text(engine_dir / "map.rs"))
    state = rewrite_engine_module("state", load_text(engine_dir / "state.rs"))

    # Strip unused engine functions for hybrid builds to save space
    if hybrid_enabled:
        map_mod = strip_unused_engine_code(map_mod)
        state = strip_unused_engine_code(state)

    input_mod = rewrite_bot_module("input", load_text(bot_dir / "input.rs"))
    eval_mod = rewrite_bot_module("eval", load_text(bot_dir / "eval.rs"))
    if hybrid_enabled:
        if random_weights_path is not None:
            weights_path = random_weights_path
        else:
            weights_rel = config_payload["hybrid"]["weights_path"]
            weights_path = Path(weights_rel)
            if not weights_path.is_absolute():
                weights_path = config_path.parent / weights_path
            weights_path = weights_path.resolve()
        features_mod = strip_training_code(
            rewrite_bot_module("features", load_text(bot_dir / "features.rs"))
        )
        if random_weights_desc:
            model_desc = random_weights_desc
        elif timing_probe:
            model_desc = _model_desc_from_weights(weights_path)
        else:
            model_desc = ""
        hybrid_mod = build_embedded_hybrid_module(
            weights_path,
            timing_probe=timing_probe,
            model_desc=model_desc,
        )
    else:
        features_mod = build_minimal_features_module()
        hybrid_mod = build_disabled_hybrid_module()
    search_mod = rewrite_bot_module("search", load_text(bot_dir / "search.rs"))
    config_mod = build_config_module(
        config_path,
        embedded_hybrid=hybrid_enabled,
        payload_override=config_payload if random_weights_path is not None else None,
    )
    main_mod = build_main_module(timing_probe=timing_probe)

    # Build engine re-exports (remove VisibilityState for hybrid builds)
    if hybrid_enabled:
        engine_reexports = [
            "    pub use self::coord::Coord;",
            "    pub use self::direction::Direction;",
            "    pub use self::map::{Grid, TileType};",
            "    pub use self::state::{",
            "        BirdCommand, BirdState, FinalResult, GameState, PlayerAction, StepResult, TerminalReason,",
            "    };",
        ]
    else:
        engine_reexports = [
            "    pub use self::coord::Coord;",
            "    pub use self::direction::Direction;",
            "    pub use self::map::{Grid, TileType};",
            "    pub use self::state::{",
            "        BirdCommand, BirdState, FinalResult, GameState, PlayerAction, StepResult, TerminalReason,",
            "        VisibilityState,",
            "    };",
        ]

    header_lines = [
        "// Generated by tools/generate_flattened_submission.py",
        f"// Source config: {config_path.relative_to(REPO_ROOT)}",
        "// This file is intended for single-file CodinGame submission builds.",
    ]
    if hybrid_enabled:
        # Unicode-encoded weights may contain bidi codepoints that trigger this lint
        header_lines.insert(0, "#![allow(text_direction_codepoint_in_literal)]")

    rendered = "\n".join(
        [
            *header_lines,
            "",
            "mod engine {",
            textwrap.indent(wrap_module("coord", coord, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("direction", direction, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("map", map_mod, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("state", state, indent="    ").rstrip(), "    "),
            *engine_reexports,
            "}",
            "",
            "mod bot {",
            textwrap.indent(wrap_module("config", config_mod, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("eval", eval_mod, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("features", features_mod, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("hybrid", hybrid_mod, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("input", input_mod, indent="    ").rstrip(), "    "),
            textwrap.indent(wrap_module("search", search_mod, indent="    ").rstrip(), "    "),
            "}",
            "",
            main_mod.rstrip(),
            "",
        ]
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rendered, encoding="utf-8")
    if hybrid_enabled:
        # Skip rustfmt and apply compact_output to stay under CodinGame 100K limit
        size = compact_output(output_path)
        print(f"compact size: {size} chars", flush=True)
        if size > 100_000:
            print(f"WARNING: output is {size} chars, exceeds 100K CodinGame limit")
    else:
        format_output(output_path)
    if not args.no_compile_check:
        compile_check(output_path)
    # Clean up temporary random weights file
    if random_weights_path is not None:
        random_weights_path.unlink(missing_ok=True)
    print(output_path)


if __name__ == "__main__":
    main()
