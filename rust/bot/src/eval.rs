use snakebot_engine::{Coord, GameState};

const BODY_WEIGHT: f64 = 120.0;
const LOSS_WEIGHT: f64 = 18.0;
const MOBILITY_WEIGHT: f64 = 7.5;
const APPLE_WEIGHT: f64 = 16.0;
const STABILITY_WEIGHT: f64 = 10.0;
const BREAKPOINT_WEIGHT: f64 = 9.0;
const FRAGILE_ATTACK_WEIGHT: f64 = 8.0;
const TERMINAL_WEIGHT: f64 = 10_000.0;

pub fn evaluate(state: &GameState, owner: usize) -> f64 {
    if state.is_game_over() {
        let final_scores = state.final_scores();
        return TERMINAL_WEIGHT * f64::from(final_scores[owner] - final_scores[1 - owner]);
    }

    let body_scores = state.body_scores();
    let body_diff = f64::from(body_scores[owner] - body_scores[1 - owner]);
    let loss_diff = f64::from(state.losses[1 - owner] - state.losses[owner]);
    let mobility = mobility_score(state, owner);
    let apple_race = apple_race_score(state, owner);
    let stability = support_stability_score(state, owner);
    let breakpoint = breakpoint_score(state, owner);
    let pressure = fragile_attack_score(state, owner);

    BODY_WEIGHT * body_diff
        + LOSS_WEIGHT * loss_diff
        + MOBILITY_WEIGHT * mobility
        + APPLE_WEIGHT * apple_race
        + STABILITY_WEIGHT * stability
        + BREAKPOINT_WEIGHT * breakpoint
        + FRAGILE_ATTACK_WEIGHT * pressure
}

fn mobility_score(state: &GameState, owner: usize) -> f64 {
    let mut total = 0.0;
    for bird in state.birds_for_player(owner).filter(|bird| bird.alive) {
        total += state.legal_commands_for_bird(bird.id).len() as f64;
    }
    for bird in state.birds_for_player(1 - owner).filter(|bird| bird.alive) {
        total -= state.legal_commands_for_bird(bird.id).len() as f64;
    }
    total
}

fn apple_race_score(state: &GameState, owner: usize) -> f64 {
    let my_heads = state
        .birds_for_player(owner)
        .filter(|bird| bird.alive)
        .map(|bird| bird.head())
        .collect::<Vec<_>>();
    let opp_heads = state
        .birds_for_player(1 - owner)
        .filter(|bird| bird.alive)
        .map(|bird| bird.head())
        .collect::<Vec<_>>();
    state
        .grid
        .apples
        .iter()
        .map(|apple| {
            let my_dist = min_distance(*apple, &my_heads);
            let opp_dist = min_distance(*apple, &opp_heads);
            f64::from(opp_dist - my_dist) / f64::from((my_dist + opp_dist + 1).max(1))
        })
        .sum()
}

fn support_stability_score(state: &GameState, owner: usize) -> f64 {
    let mut score = 0.0;
    for bird in state.birds.iter().filter(|bird| bird.alive) {
        let supported = bird
            .body
            .iter()
            .any(|coord| has_solid_below(state, bird.id, *coord));
        let value = if supported { 1.0 } else { -1.0 };
        if bird.owner == owner {
            score += value;
        } else {
            score -= value;
        }
    }
    score
}

fn breakpoint_score(state: &GameState, owner: usize) -> f64 {
    let own = state
        .birds_for_player(owner)
        .filter(|bird| bird.alive)
        .map(|bird| usize::from(bird.length() >= 4) as f64)
        .sum::<f64>();
    let opp = state
        .birds_for_player(1 - owner)
        .filter(|bird| bird.alive)
        .map(|bird| usize::from(bird.length() >= 4) as f64)
        .sum::<f64>();
    own - opp
}

fn fragile_attack_score(state: &GameState, owner: usize) -> f64 {
    let my_heads = state
        .birds_for_player(owner)
        .filter(|bird| bird.alive)
        .map(|bird| bird.head())
        .collect::<Vec<_>>();
    state
        .birds_for_player(1 - owner)
        .filter(|bird| bird.alive && bird.length() <= 3)
        .map(|bird| {
            let closest = min_distance(bird.head(), &my_heads);
            1.0 / f64::from(closest + 1)
        })
        .sum()
}

fn has_solid_below(state: &GameState, bird_id: i32, coord: Coord) -> bool {
    let below = coord.add(0, 1);
    if state.grid.get(below) == Some(snakebot_engine::TileType::Wall) {
        return true;
    }
    if state.grid.apples.contains(&below) {
        return true;
    }
    state
        .birds
        .iter()
        .filter(|bird| bird.alive && bird.id != bird_id)
        .any(|bird| bird.body.contains(&below))
}

fn min_distance(target: Coord, coords: &[Coord]) -> i32 {
    coords
        .iter()
        .map(|coord| coord.manhattan_to(target))
        .min()
        .unwrap_or(99)
}
