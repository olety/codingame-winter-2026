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
    source = source.replace("Serialize, Deserialize, ", "")
    source = source.replace("Deserialize, Serialize, ", "")
    source = source.replace(", Serialize, Deserialize", "")
    source = source.replace(", Deserialize, Serialize", "")
    source = source.replace("Serialize, Deserialize", "")
    source = source.replace("Deserialize, Serialize", "")
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
            "use snakebot_engine::{BirdCommand, Coord, Direction, FinalResult, GameState, OracleState, PlayerAction, TileType};": "use crate::engine::{BirdCommand, Coord, Direction, FinalResult, GameState, OracleState, PlayerAction, TileType};",
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


def build_config_module(config_path: Path) -> str:
    payload = json.loads(load_text(config_path))
    eval_payload = payload["eval"]
    search_payload = payload["search"]
    hybrid_payload = payload.get("hybrid")
    name_literal = json.dumps(payload["name"])
    hybrid_literal = "None"
    if hybrid_payload:
        weights_path = json.dumps(hybrid_payload.get("weights_path"))
        hybrid_literal = textwrap.dedent(
            f"""
            Some(HybridConfig {{
                weights_path: {weights_path}.map(str::to_owned),
                prior_mix: {hybrid_payload.get("prior_mix", 0.0)},
                leaf_mix: {hybrid_payload.get("leaf_mix", 0.0)},
                value_scale: {hybrid_payload.get("value_scale", 48.0)},
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
        }}

        impl Default for HybridConfig {{
            fn default() -> Self {{
                Self {{
                    weights_path: None,
                    prior_mix: 0.0,
                    leaf_mix: 0.0,
                    value_scale: 48.0,
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
        raise SystemExit(
            "flattened single-file generation does not yet support hybrid-enabled configs; "
            "promote a search-only config or extend the generator to embed weights first"
        )
    features_mod = build_minimal_features_module()
    hybrid_mod = build_disabled_hybrid_module()
    search_mod = rewrite_bot_module("search", load_text(bot_dir / "search.rs"))
    config_mod = build_config_module(config_path)
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
