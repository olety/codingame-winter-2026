use std::time::{Duration, Instant};

use snakebot_engine::{GameState, PlayerAction};

use crate::features::{encode_training_row, TrainingRow};
use crate::search::choose_action;

#[derive(Clone, Copy, Debug)]
pub struct SelfPlayConfig {
    pub max_turns: usize,
    pub search_time_ms: u64,
}

impl Default for SelfPlayConfig {
    fn default() -> Self {
        Self {
            max_turns: 200,
            search_time_ms: 0,
        }
    }
}

pub fn play_and_collect(initial_state: GameState, config: SelfPlayConfig) -> Vec<TrainingRow> {
    let mut state = initial_state;
    let mut positions = Vec::new();

    while !state.is_game_over() && state.turn < config.max_turns as i32 {
        positions.push((state.clone(), 0_usize));
        positions.push((state.clone(), 1_usize));

        let p0 = choose_selfplay_action(&state, 0, config.search_time_ms);
        let p1 = choose_selfplay_action(&state, 1, config.search_time_ms);
        state.step(&p0, &p1);
    }

    let final_scores = state.final_scores();
    positions
        .into_iter()
        .map(|(position, owner)| encode_training_row(&position, owner, final_scores))
        .collect()
}

fn choose_selfplay_action(state: &GameState, owner: usize, search_time_ms: u64) -> PlayerAction {
    if search_time_ms == 0 {
        return crate::search::choose_action_unlimited(state, owner).action;
    }
    choose_action(
        state,
        owner,
        Instant::now() + Duration::from_millis(search_time_ms),
    )
    .action
}

