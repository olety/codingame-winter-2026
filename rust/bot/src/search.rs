use std::time::{Duration, Instant};

use snakebot_engine::{BirdCommand, Direction, GameState, PlayerAction};

use crate::eval::evaluate;

#[derive(Clone, Debug)]
pub struct SearchOutcome {
    pub action: PlayerAction,
}

pub fn choose_action(state: &GameState, owner: usize, deadline: Instant) -> SearchOutcome {
    choose_action_with_budget(state, owner, Some(deadline))
}

pub fn choose_action_unlimited(state: &GameState, owner: usize) -> SearchOutcome {
    choose_action_with_budget(state, owner, None)
}

fn choose_action_with_budget(
    state: &GameState,
    owner: usize,
    deadline: Option<Instant>,
) -> SearchOutcome {
    let my_actions = state.legal_joint_actions(owner);
    let opp_actions = state.legal_joint_actions(1 - owner);

    let mut best_action = PlayerAction::default();
    let mut best_score = f64::NEG_INFINITY;
    let mut best_mean = f64::NEG_INFINITY;

    for my_action in my_actions {
        if expired(deadline) {
            break;
        }

        let mut worst_score = f64::INFINITY;
        let mut mean_score = 0.0;
        let mut count = 0_usize;

        for opp_action in opp_actions.iter() {
            if expired(deadline) {
                break;
            }
            let mut next = state.clone();
            if owner == 0 {
                next.step(&my_action, opp_action);
            } else {
                next.step(opp_action, &my_action);
            }
            let score = evaluate(&next, owner);
            worst_score = worst_score.min(score);
            mean_score += score;
            count += 1;
        }

        if count == 0 {
            continue;
        }

        mean_score /= count as f64;
        if worst_score > best_score || (worst_score == best_score && mean_score > best_mean) {
            best_score = worst_score;
            best_mean = mean_score;
            best_action = my_action;
        }
    }

    let _ = best_score;
    SearchOutcome {
        action: best_action,
    }
}

fn expired(deadline: Option<Instant>) -> bool {
    deadline.is_some_and(|deadline| Instant::now() >= deadline)
}

pub fn render_action(action: &PlayerAction) -> String {
    let mut commands = action
        .per_bird
        .iter()
        .filter_map(|(bird_id, command)| match command {
            BirdCommand::Keep => None,
            BirdCommand::Turn(direction) => {
                Some(format!("{} {}", bird_id, direction_to_word(*direction)))
            }
        })
        .collect::<Vec<_>>();
    commands.sort();
    if commands.is_empty() {
        "WAIT".to_owned()
    } else {
        commands.join(";")
    }
}

pub fn safe_deadline() -> Instant {
    Instant::now() + Duration::from_millis(42)
}

fn direction_to_word(direction: Direction) -> &'static str {
    match direction {
        Direction::North => "UP",
        Direction::East => "RIGHT",
        Direction::South => "DOWN",
        Direction::West => "LEFT",
        Direction::Unset => "UP",
    }
}

#[cfg(test)]
mod tests {
    use snakebot_engine::{Coord, Direction, GameState, Grid, TileType};

    use super::{choose_action, choose_action_unlimited, render_action, safe_deadline};

    #[test]
    fn render_wait_when_no_turns_needed() {
        assert_eq!(render_action(&Default::default()), "WAIT");
    }

    #[test]
    fn search_returns_non_empty_command() {
        let mut grid = Grid::new(5, 5);
        for x in 0..5 {
            grid.set(Coord::new(x, 4), TileType::Wall);
        }
        let mut state = GameState::new(grid);
        state.add_bird(
            0,
            0,
            vec![Coord::new(1, 2), Coord::new(1, 3), Coord::new(1, 4)],
            Some(Direction::North),
        );
        state.add_bird(
            1,
            1,
            vec![Coord::new(3, 2), Coord::new(3, 3), Coord::new(3, 4)],
            Some(Direction::North),
        );
        let outcome = choose_action(&state, 0, safe_deadline());
        let command = render_action(&outcome.action);
        assert!(!command.is_empty());
    }

    #[test]
    fn unlimited_search_matches_interface() {
        let mut grid = Grid::new(5, 5);
        for x in 0..5 {
            grid.set(Coord::new(x, 4), TileType::Wall);
        }
        let mut state = GameState::new(grid);
        state.add_bird(
            0,
            0,
            vec![Coord::new(1, 2), Coord::new(1, 3), Coord::new(1, 4)],
            Some(Direction::North),
        );
        state.add_bird(
            1,
            1,
            vec![Coord::new(3, 2), Coord::new(3, 3), Coord::new(3, 4)],
            Some(Direction::North),
        );
        let command = render_action(&choose_action_unlimited(&state, 0).action);
        assert!(!command.is_empty());
    }
}
