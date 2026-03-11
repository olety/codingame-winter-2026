use snakebot_engine::{FinalResult, GameState, PlayerAction};

use crate::config::BotConfig;
use crate::search::{choose_action, SearchBudget, SearchOutcome};

#[derive(Clone, Debug)]
pub struct SelfPlayConfig {
    pub max_turns: usize,
    pub budget: SearchBudget,
    pub bot_config: BotConfig,
}

impl Default for SelfPlayConfig {
    fn default() -> Self {
        Self {
            max_turns: 200,
            budget: SearchBudget::ExtraNodesAfterRoot(5_000),
            bot_config: BotConfig::default(),
        }
    }
}

#[derive(Clone, Debug)]
pub struct CollectedPosition {
    pub state: GameState,
    pub owner: usize,
    pub turn: i32,
    pub outcome: SearchOutcome,
}

#[derive(Clone, Debug)]
pub struct CollectedGame {
    pub positions: Vec<CollectedPosition>,
    pub final_result: FinalResult,
}

pub fn play_and_collect(initial_state: GameState, config: &SelfPlayConfig) -> CollectedGame {
    let mut state = initial_state;
    let mut positions = Vec::new();

    while !state.is_terminal(config.max_turns as i32) {
        let p0 = choose_action(&state, 0, &config.bot_config, config.budget);
        positions.push(CollectedPosition {
            state: state.clone(),
            owner: 0,
            turn: state.turn,
            outcome: p0.clone(),
        });
        let p1 = choose_action(&state, 1, &config.bot_config, config.budget);
        positions.push(CollectedPosition {
            state: state.clone(),
            owner: 1,
            turn: state.turn,
            outcome: p1.clone(),
        });

        state.step(&p0.action, &p1.action);
    }

    let final_result = state.final_result(config.max_turns as i32);
    CollectedGame {
        positions,
        final_result,
    }
}

pub fn choose_selfplay_action(
    state: &GameState,
    owner: usize,
    config: &SelfPlayConfig,
) -> PlayerAction {
    choose_action(state, owner, &config.bot_config, config.budget).action
}
