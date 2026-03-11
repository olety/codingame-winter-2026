use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use snakebot_engine::{BirdCommand, Direction, GameState, PlayerAction};

use crate::config::BotConfig;
use crate::eval::evaluate;

const CONTEST_MAX_TURNS: i32 = 200;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SearchBudget {
    TimeMs(u64),
    ExtraNodesAfterRoot(u64),
}

#[derive(Clone, Debug, Default, Eq, PartialEq, Serialize, Deserialize)]
pub struct SearchStats {
    pub elapsed_ms: u64,
    pub root_pairs: u64,
    pub extra_nodes: u64,
    pub cutoffs: u64,
    pub tt_hits: u64,
    pub my_action_count: usize,
    pub opp_action_count: usize,
    pub deepened_actions: usize,
}

#[derive(Clone, Debug)]
pub struct SearchOutcome {
    pub action: PlayerAction,
    pub action_id: usize,
    pub action_count: usize,
    pub root_values: Vec<f32>,
    pub score: f64,
    pub mean_score: f64,
    pub stats: SearchStats,
}

#[derive(Clone)]
struct RootResponse {
    opp_index: usize,
    score: f64,
}

#[derive(Clone)]
struct RootAnalysis {
    action: PlayerAction,
    action_id: usize,
    worst_score: f64,
    mean_score: f64,
    responses: Vec<RootResponse>,
}

pub fn live_budget_for_turn(config: &BotConfig, turn: i32) -> SearchBudget {
    if turn == 0 {
        SearchBudget::TimeMs(config.search.first_turn_ms)
    } else {
        SearchBudget::TimeMs(config.search.later_turn_ms)
    }
}

pub fn choose_action(
    state: &GameState,
    owner: usize,
    config: &BotConfig,
    budget: SearchBudget,
) -> SearchOutcome {
    let started = Instant::now();
    let deadline = deadline_for_budget(started, budget);
    let allow_cutoff = matches!(budget, SearchBudget::TimeMs(_));
    let mut extra_nodes_remaining = match budget {
        SearchBudget::TimeMs(_) => u64::MAX,
        SearchBudget::ExtraNodesAfterRoot(nodes) => nodes,
    };

    let my_actions = ordered_joint_actions(state, owner);
    let opp_actions = ordered_joint_actions(state, 1 - owner);
    let my_action_count = my_actions.len();
    let opp_action_count = opp_actions.len();
    let default_action = PlayerAction::default();

    let mut my_order = (0..my_actions.len())
        .map(|idx| {
            (
                idx,
                action_prior(state, owner, &my_actions[idx], &default_action, config),
            )
        })
        .collect::<Vec<_>>();
    my_order.sort_by(|left, right| right.1.total_cmp(&left.1));

    let mut opp_order = (0..opp_actions.len())
        .map(|idx| {
            (
                idx,
                action_prior(state, owner, &default_action, &opp_actions[idx], config),
            )
        })
        .collect::<Vec<_>>();
    opp_order.sort_by(|left, right| left.1.total_cmp(&right.1));

    let mut stats = SearchStats {
        my_action_count,
        opp_action_count,
        ..SearchStats::default()
    };
    let mut root_analyses = Vec::with_capacity(my_actions.len());
    let mut root_values = vec![f32::NEG_INFINITY; my_actions.len()];
    let mut best = RootAnalysis {
        action: PlayerAction::default(),
        action_id: 0,
        worst_score: f64::NEG_INFINITY,
        mean_score: f64::NEG_INFINITY,
        responses: Vec::new(),
    };

    'outer: for (my_index, _) in my_order {
        if expired(deadline) {
            break;
        }

        let my_action = my_actions[my_index].clone();
        let mut worst_score = f64::INFINITY;
        let mut mean_score = 0.0;
        let mut evaluated = 0_usize;
        let mut responses = Vec::with_capacity(opp_actions.len());

        for (opp_index, _) in opp_order.iter().copied() {
            if expired(deadline) {
                break 'outer;
            }

            let next = simulate_state(state, owner, &my_action, &opp_actions[opp_index]);
            let score = evaluate(&next, owner, CONTEST_MAX_TURNS, &config.eval);
            stats.root_pairs += 1;
            worst_score = worst_score.min(score);
            mean_score += score;
            evaluated += 1;
            responses.push(RootResponse { opp_index, score });

            if allow_cutoff && worst_score < best.worst_score {
                stats.cutoffs += 1;
                break;
            }
        }

        if evaluated == 0 {
            continue;
        }

        mean_score /= evaluated as f64;
        root_values[my_index] = worst_score as f32;

        let analysis = RootAnalysis {
            action: my_action,
            action_id: my_index,
            worst_score,
            mean_score,
            responses,
        };
        if analysis.worst_score > best.worst_score
            || (analysis.worst_score == best.worst_score && analysis.mean_score > best.mean_score)
        {
            best = analysis.clone();
        }
        root_analyses.push(analysis);
    }

    if root_analyses.is_empty() {
        stats.elapsed_ms = started.elapsed().as_millis() as u64;
        return SearchOutcome {
            action: PlayerAction::default(),
            action_id: 0,
            action_count: my_action_count.max(1),
            root_values,
            score: f64::NEG_INFINITY,
            mean_score: f64::NEG_INFINITY,
            stats,
        };
    }

    root_analyses.sort_by(|left, right| {
        right
            .worst_score
            .total_cmp(&left.worst_score)
            .then_with(|| right.mean_score.total_cmp(&left.mean_score))
    });

    for analysis in root_analyses
        .iter()
        .take(config.search.deepen_top_my.min(root_analyses.len()))
    {
        if expired(deadline) || extra_nodes_remaining == 0 {
            break;
        }
        stats.deepened_actions += 1;

        let mut candidate_score = analysis.worst_score;
        let mut sorted_responses = analysis.responses.clone();
        sorted_responses.sort_by(|left, right| left.score.total_cmp(&right.score));

        for response in sorted_responses
            .into_iter()
            .take(config.search.deepen_top_opp.min(analysis.responses.len()))
        {
            if expired(deadline) || extra_nodes_remaining == 0 {
                break;
            }
            let next = simulate_state(
                state,
                owner,
                &analysis.action,
                &opp_actions[response.opp_index],
            );
            let child_score = deepen_branch(
                &next,
                owner,
                config,
                deadline,
                &mut extra_nodes_remaining,
                &mut stats,
            );
            candidate_score = candidate_score.min(child_score);
        }

        if candidate_score > best.worst_score
            || (candidate_score == best.worst_score && analysis.mean_score > best.mean_score)
        {
            best = RootAnalysis {
                action: analysis.action.clone(),
                action_id: analysis.action_id,
                worst_score: candidate_score,
                mean_score: analysis.mean_score,
                responses: analysis.responses.clone(),
            };
        }
    }

    stats.elapsed_ms = started.elapsed().as_millis() as u64;
    SearchOutcome {
        action: best.action,
        action_id: best.action_id,
        action_count: my_action_count.max(1),
        root_values,
        score: best.worst_score,
        mean_score: best.mean_score,
        stats,
    }
}

pub fn choose_action_unlimited(
    state: &GameState,
    owner: usize,
    config: &BotConfig,
) -> SearchOutcome {
    choose_action(
        state,
        owner,
        config,
        SearchBudget::ExtraNodesAfterRoot(u64::MAX / 4),
    )
}

fn deepen_branch(
    state: &GameState,
    owner: usize,
    config: &BotConfig,
    deadline: Option<Instant>,
    extra_nodes_remaining: &mut u64,
    stats: &mut SearchStats,
) -> f64 {
    let my_actions = ordered_joint_actions(state, owner);
    let opp_actions = ordered_joint_actions(state, 1 - owner);
    let default_action = PlayerAction::default();

    let mut my_order = (0..my_actions.len())
        .map(|idx| {
            (
                idx,
                action_prior(state, owner, &my_actions[idx], &default_action, config),
            )
        })
        .collect::<Vec<_>>();
    my_order.sort_by(|left, right| right.1.total_cmp(&left.1));

    let mut opp_order = (0..opp_actions.len())
        .map(|idx| {
            (
                idx,
                action_prior(state, owner, &default_action, &opp_actions[idx], config),
            )
        })
        .collect::<Vec<_>>();
    opp_order.sort_by(|left, right| left.1.total_cmp(&right.1));

    let mut worst = f64::INFINITY;
    for (my_index, _) in my_order
        .into_iter()
        .take(config.search.deepen_child_my.min(my_actions.len()))
    {
        if expired(deadline) || *extra_nodes_remaining == 0 {
            break;
        }
        let mut child_best = f64::INFINITY;
        for (opp_index, _) in opp_order
            .iter()
            .copied()
            .take(config.search.deepen_child_opp.min(opp_actions.len()))
        {
            if expired(deadline) || *extra_nodes_remaining == 0 {
                break;
            }
            *extra_nodes_remaining -= 1;
            stats.extra_nodes += 1;
            let next = simulate_state(state, owner, &my_actions[my_index], &opp_actions[opp_index]);
            let score = evaluate(&next, owner, CONTEST_MAX_TURNS, &config.eval);
            child_best = child_best.min(score);
        }
        worst = worst.min(child_best);
    }

    if worst.is_finite() {
        worst
    } else {
        evaluate(state, owner, CONTEST_MAX_TURNS, &config.eval)
    }
}

fn action_prior(
    state: &GameState,
    owner: usize,
    my_action: &PlayerAction,
    opp_action: &PlayerAction,
    config: &BotConfig,
) -> f64 {
    let next = simulate_state(state, owner, my_action, opp_action);
    evaluate(&next, owner, CONTEST_MAX_TURNS, &config.eval)
}

fn simulate_state(
    state: &GameState,
    owner: usize,
    my_action: &PlayerAction,
    opp_action: &PlayerAction,
) -> GameState {
    let mut next = state.clone();
    if owner == 0 {
        next.step(my_action, opp_action);
    } else {
        next.step(opp_action, my_action);
    }
    next
}

fn deadline_for_budget(started: Instant, budget: SearchBudget) -> Option<Instant> {
    match budget {
        SearchBudget::TimeMs(ms) => Some(started + Duration::from_millis(ms)),
        SearchBudget::ExtraNodesAfterRoot(_) => None,
    }
}

fn expired(deadline: Option<Instant>) -> bool {
    deadline.is_some_and(|deadline| Instant::now() >= deadline)
}

fn ordered_joint_actions(state: &GameState, owner: usize) -> Vec<PlayerAction> {
    let birds = state
        .birds_for_player(owner)
        .filter(|bird| bird.alive)
        .map(|bird| bird.id)
        .collect::<Vec<_>>();
    let mut result = Vec::new();
    let mut current = PlayerAction::default();
    enumerate_actions(state, &birds, 0, &mut current, &mut result);
    if result.is_empty() {
        result.push(PlayerAction::default());
    }
    result
}

fn enumerate_actions(
    state: &GameState,
    birds: &[i32],
    idx: usize,
    current: &mut PlayerAction,
    output: &mut Vec<PlayerAction>,
) {
    if idx == birds.len() {
        output.push(current.clone());
        return;
    }

    let bird_id = birds[idx];
    for command in ordered_commands_for_bird(state, bird_id) {
        match command {
            BirdCommand::Keep => {
                current.per_bird.remove(&bird_id);
            }
            BirdCommand::Turn(direction) => {
                current
                    .per_bird
                    .insert(bird_id, BirdCommand::Turn(direction));
            }
        }
        enumerate_actions(state, birds, idx + 1, current, output);
    }
    current.per_bird.remove(&bird_id);
}

fn ordered_commands_for_bird(state: &GameState, bird_id: i32) -> Vec<BirdCommand> {
    let Some(bird) = state
        .birds
        .iter()
        .find(|bird| bird.id == bird_id && bird.alive)
    else {
        return Vec::new();
    };
    let facing = bird.facing();
    let mut commands = vec![BirdCommand::Keep];
    if facing == Direction::Unset {
        for direction in Direction::ALL {
            commands.push(BirdCommand::Turn(direction));
        }
        return commands;
    }

    commands.push(BirdCommand::Turn(turn_left(facing)));
    commands.push(BirdCommand::Turn(turn_right(facing)));
    commands
}

fn turn_left(direction: Direction) -> Direction {
    match direction {
        Direction::North => Direction::West,
        Direction::West => Direction::South,
        Direction::South => Direction::East,
        Direction::East => Direction::North,
        Direction::Unset => Direction::Unset,
    }
}

fn turn_right(direction: Direction) -> Direction {
    match direction {
        Direction::North => Direction::East,
        Direction::East => Direction::South,
        Direction::South => Direction::West,
        Direction::West => Direction::North,
        Direction::Unset => Direction::Unset,
    }
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

    use crate::config::BotConfig;

    use super::{
        choose_action, choose_action_unlimited, live_budget_for_turn, render_action, SearchBudget,
    };

    #[test]
    fn render_wait_when_no_turns_needed() {
        assert_eq!(render_action(&Default::default()), "WAIT");
    }

    #[test]
    fn live_budget_uses_opening_and_later_windows() {
        let config = BotConfig::default();
        assert_eq!(live_budget_for_turn(&config, 0), SearchBudget::TimeMs(850));
        assert_eq!(live_budget_for_turn(&config, 1), SearchBudget::TimeMs(40));
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
        let outcome = choose_action(
            &state,
            0,
            &BotConfig::default(),
            SearchBudget::ExtraNodesAfterRoot(64),
        );
        let command = render_action(&outcome.action);
        assert!(!command.is_empty());
    }

    #[test]
    fn deterministic_budget_is_repeatable() {
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
        let config = BotConfig::default();
        let left = choose_action(&state, 0, &config, SearchBudget::ExtraNodesAfterRoot(64));
        let right = choose_action(&state, 0, &config, SearchBudget::ExtraNodesAfterRoot(64));
        assert_eq!(render_action(&left.action), render_action(&right.action));
        assert_eq!(left.stats.root_pairs, right.stats.root_pairs);
        assert_eq!(left.stats.extra_nodes, right.stats.extra_nodes);
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
        let command =
            render_action(&choose_action_unlimited(&state, 0, &BotConfig::default()).action);
        assert!(!command.is_empty());
    }
}
