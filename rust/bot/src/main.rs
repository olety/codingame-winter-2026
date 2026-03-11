use std::io::{self, BufRead, BufReader, Write};

use snakebot_bot::config::BotConfig;
use snakebot_bot::input::{BotIo, FrameObservation};
use snakebot_bot::search::{choose_action, live_budget_for_turn, render_action};
use snakebot_engine::{GameState, PlayerAction};

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
            config: BotConfig::default(),
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
