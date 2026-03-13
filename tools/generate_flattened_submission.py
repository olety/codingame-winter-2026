#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
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


def strip_training_code(source: str) -> str:
    """Remove training-only structs and functions from features.rs.

    These reference types (OracleState, FinalResult, SearchStats, serde_json)
    that are not available in the flattened submission.
    """
    # Remove TrainingRow struct (pub struct TrainingRow { ... })
    for struct_name in ("TrainingRow", "TrainingMetadata"):
        pattern = rf"(?:#\[derive\([^\)]*\)\]\n)?pub struct {struct_name} \{{[^}}]*\}}"
        source = re.sub(pattern, "", source, flags=re.DOTALL)

    # Remove encode_training_row function
    for func_name in ("encode_training_row", "stable_hash"):
        pattern = rf"pub fn {func_name}\b"
        alt_pattern = rf"fn {func_name}\b"
        for pat in (pattern, alt_pattern):
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

    # Remove use crate::search::SearchStats (may already be rewritten to super::)
    source = re.sub(r"^use super::search::SearchStats;\n", "", source, flags=re.MULTILINE)

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


def wrap_module(name: str, body: str) -> str:
    return f"pub mod {name} {{\n{textwrap.indent(body.rstrip(), '    ')}\n}}\n"


def build_config_module(config_path: Path, *, embedded_hybrid: bool = False) -> str:
    payload = json.loads(load_text(config_path))
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


def _format_float_array(values: list[float], name: str) -> str:
    """Format a list of floats as a Rust static const array."""
    items = ", ".join(f"{v:.8e}" for v in values)
    return f"static {name}: [f32; {len(values)}] = [{items}];"


def _build_conv_layer_statics(prefix: str, layer: dict) -> tuple[str, str]:
    """Return (static declarations, constructor expression) for a ConvLayer."""
    w_name = f"{prefix}_W"
    b_name = f"{prefix}_B"
    statics = "\n".join([
        _format_float_array(layer["weights"], w_name),
        _format_float_array(layer["bias"], b_name),
    ])
    constructor = textwrap.dedent(f"""
        ConvLayer {{
            out_channels: {layer['out_channels']},
            in_channels: {layer['in_channels']},
            kernel_size: {layer['kernel_size']},
            weights: &{w_name},
            bias: &{b_name},
        }}
    """).strip()
    return statics, constructor


def _build_linear_layer_statics(prefix: str, layer: dict) -> tuple[str, str]:
    """Return (static declarations, constructor expression) for a LinearLayer."""
    w_name = f"{prefix}_W"
    b_name = f"{prefix}_B"
    statics = "\n".join([
        _format_float_array(layer["weights"], w_name),
        _format_float_array(layer["bias"], b_name),
    ])
    constructor = textwrap.dedent(f"""
        LinearLayer {{
            out_features: {layer['out_features']},
            in_features: {layer['in_features']},
            weights: &{w_name},
            bias: &{b_name},
        }}
    """).strip()
    return statics, constructor


def build_embedded_hybrid_module(weights_path: Path) -> str:
    """Build a hybrid module with neural network weights embedded as static arrays."""
    raw = weights_path.read_text(encoding="utf-8")
    model = json.loads(raw)

    # Collect all static arrays
    all_statics: list[str] = []
    constructors: dict[str, str] = {}

    for layer_name in ("conv1", "conv2"):
        s, c = _build_conv_layer_statics(layer_name.upper(), model[layer_name])
        all_statics.append(s)
        constructors[layer_name] = c

    has_conv3 = model.get("conv3") is not None
    if has_conv3:
        s, c = _build_conv_layer_statics("CONV3", model["conv3"])
        all_statics.append(s)
        constructors["conv3"] = c

    for layer_name in ("policy", "value"):
        s, c = _build_linear_layer_statics(layer_name.upper(), model[layer_name])
        all_statics.append(s)
        constructors[layer_name] = c

    conv3_field = f"conv3: Some({constructors['conv3']})," if has_conv3 else "conv3: None,"

    statics_block = "\n\n".join(all_statics)

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

        {statics_block}

        fn get_embedded_model() -> &'static TinyHybridWeights {{
            static MODEL: OnceLock<TinyHybridWeights> = OnceLock::new();
            MODEL.get_or_init(|| TinyHybridWeights {{
                input_channels: {model['input_channels']},
                scalar_features: {model['scalar_features']},
                conv1: {constructors['conv1']},
                conv2: {constructors['conv2']},
                {conv3_field}
                policy: {constructors['policy']},
                value: {constructors['value']},
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
                                        let w_idx = ((oc * self.in_channels + ic) * self.kernel_size + ky)
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


def build_main_module() -> str:
    return textwrap.dedent(
        """
        use std::io::{self, BufRead, BufReader, Write};

        use bot::config::BotConfig;
        use bot::input::{BotIo, FrameObservation};
        use bot::search::{choose_action, live_budget_for_turn, render_action};
        use engine::{GameState, PlayerAction};

        fn main() -> io::Result<()> {
            let stdin = io::stdin();
            let stdout = io::stdout();
            let mut reader = BufReader::new(stdin.lock());
            let mut writer = io::BufWriter::new(stdout.lock());
            let bot = Bot::new(BotIo::read(&mut reader)?);
            bot.run(&mut reader, &mut writer)
        }

        struct Bot {
            config: BotConfig,
            io: BotIo,
            state: Option<GameState>,
            last_my_action: PlayerAction,
        }

        impl Bot {
            fn new(io: BotIo) -> Self {
                Self {
                    config: BotConfig::embedded(),
                    io,
                    state: None,
                    last_my_action: PlayerAction::default(),
                }
            }

            fn run(mut self, reader: &mut impl BufRead, writer: &mut impl Write) -> io::Result<()> {
                let debug_stats = std::env::var_os("SNAKEBOT_DEBUG_STATS").is_some();
                while let Some(frame) = self.io.read_frame(reader)? {
                    let state = self.reconcile_state(&frame);
                    let budget = live_budget_for_turn(&self.config, state.turn);
                    let outcome = choose_action(&state, self.io.player_index, &self.config, budget);
                    if debug_stats {
                        eprintln!(
                            "turn={} elapsed_ms={} root_pairs={} extra_nodes={} cutoffs={}",
                            state.turn,
                            outcome.stats.elapsed_ms,
                            outcome.stats.root_pairs,
                            outcome.stats.extra_nodes,
                            outcome.stats.cutoffs
                        );
                    }
                    let command = render_action(&outcome.action);
                    writeln!(writer, "{command}")?;
                    writer.flush()?;
                    self.last_my_action = outcome.action;
                    self.state = Some(state);
                }
                Ok(())
            }

            fn reconcile_state(&self, frame: &FrameObservation) -> GameState {
                let observed_signature = self.io.observation_signature(frame);
                let fallback = if let Some(previous) = &self.state {
                    self.io
                        .build_visible_state(frame, previous.losses, previous.turn + 1)
                } else {
                    self.io.build_visible_state(frame, [0, 0], 0)
                };

                let Some(previous) = &self.state else {
                    return fallback;
                };

                let opponent = 1 - self.io.player_index;
                for opp_action in previous.legal_joint_actions(opponent) {
                    let mut simulated = previous.clone();
                    if self.io.player_index == 0 {
                        simulated.step(&self.last_my_action, &opp_action);
                    } else {
                        simulated.step(&opp_action, &self.last_my_action);
                    }
                    if self.io.visible_signature(&simulated) == observed_signature {
                        return simulated;
                    }
                }

                if std::env::var_os("SNAKEBOT_STRICT_RECONCILE").is_some() {
                    panic!("reconcile_state fell back to visible reconstruction");
                }

                fallback
            }
        }
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


def main() -> None:
    args = parse_args()
    config_path = args.config.resolve()
    output_path = args.output.resolve()
    config_payload = json.loads(load_text(config_path))
    hybrid_enabled = bool(config_payload.get("hybrid"))

    engine_dir = REPO_ROOT / "rust/engine/src"
    bot_dir = REPO_ROOT / "rust/bot/src"

    coord = rewrite_engine_module("coord", load_text(engine_dir / "coord.rs"))
    direction = rewrite_engine_module("direction", load_text(engine_dir / "direction.rs"))
    map_mod = rewrite_engine_module("map", load_text(engine_dir / "map.rs"))
    state = rewrite_engine_module("state", load_text(engine_dir / "state.rs"))
    input_mod = rewrite_bot_module("input", load_text(bot_dir / "input.rs"))
    eval_mod = rewrite_bot_module("eval", load_text(bot_dir / "eval.rs"))
    if hybrid_enabled:
        weights_rel = config_payload["hybrid"]["weights_path"]
        weights_path = Path(weights_rel)
        if not weights_path.is_absolute():
            weights_path = config_path.parent / weights_path
        weights_path = weights_path.resolve()
        features_mod = strip_training_code(
            rewrite_bot_module("features", load_text(bot_dir / "features.rs"))
        )
        hybrid_mod = build_embedded_hybrid_module(weights_path)
    else:
        features_mod = build_minimal_features_module()
        hybrid_mod = build_disabled_hybrid_module()
    search_mod = rewrite_bot_module("search", load_text(bot_dir / "search.rs"))
    config_mod = build_config_module(config_path, embedded_hybrid=hybrid_enabled)
    main_mod = build_main_module()

    rendered = "\n".join(
        [
            "// Generated by tools/generate_flattened_submission.py",
            f"// Source config: {config_path.relative_to(REPO_ROOT)}",
            "// This file is intended for single-file CodinGame submission builds.",
            "",
            "mod engine {",
            textwrap.indent(wrap_module("coord", coord).rstrip(), "    "),
            textwrap.indent(wrap_module("direction", direction).rstrip(), "    "),
            textwrap.indent(wrap_module("map", map_mod).rstrip(), "    "),
            textwrap.indent(wrap_module("state", state).rstrip(), "    "),
            "    pub use self::coord::Coord;",
            "    pub use self::direction::Direction;",
            "    pub use self::map::{Grid, TileType};",
            "    pub use self::state::{",
            "        BirdCommand, BirdState, FinalResult, GameState, PlayerAction, StepResult, TerminalReason,",
            "        VisibilityState,",
            "    };",
            "}",
            "",
            "mod bot {",
            textwrap.indent(wrap_module("config", config_mod).rstrip(), "    "),
            textwrap.indent(wrap_module("eval", eval_mod).rstrip(), "    "),
            textwrap.indent(wrap_module("features", features_mod).rstrip(), "    "),
            textwrap.indent(wrap_module("hybrid", hybrid_mod).rstrip(), "    "),
            textwrap.indent(wrap_module("input", input_mod).rstrip(), "    "),
            textwrap.indent(wrap_module("search", search_mod).rstrip(), "    "),
            "}",
            "",
            main_mod.rstrip(),
            "",
        ]
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rendered, encoding="utf-8")
    format_output(output_path)
    if not args.no_compile_check:
        compile_check(output_path)
    print(output_path)


if __name__ == "__main__":
    main()
