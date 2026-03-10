use serde::Serialize;
use snakebot_engine::{Coord, GameState, TileType};

pub const GRID_CHANNELS: usize = 8;
pub const SCALAR_FEATURES: usize = 6;
const VALUE_SCALE: f32 = 12.0;

#[derive(Clone, Debug, Serialize)]
pub struct EncodedPosition {
    pub grid: Vec<Vec<Vec<f32>>>,
    pub scalars: Vec<f32>,
}

#[derive(Clone, Debug, Serialize)]
pub struct TrainingRow {
    pub grid: Vec<Vec<Vec<f32>>>,
    pub scalars: Vec<f32>,
    pub value: f32,
    pub weight: f32,
}

pub fn encode_position(state: &GameState, owner: usize) -> EncodedPosition {
    let width = state.grid.width as usize;
    let height = state.grid.height as usize;
    let mut grid = vec![vec![vec![0.0_f32; width]; height]; GRID_CHANNELS];

    for coord in state.grid.coords() {
        let view = canonical_coord(state, owner, coord);
        let (x, y) = (view.x as usize, view.y as usize);
        if state.grid.get(coord) == Some(TileType::Wall) {
            grid[0][y][x] = 1.0;
        }
    }

    for apple in &state.grid.apples {
        let view = canonical_coord(state, owner, *apple);
        grid[1][view.y as usize][view.x as usize] = 1.0;
    }

    for bird in state.birds.iter().filter(|bird| bird.alive) {
        let channels = if bird.owner == owner { (2, 3, 6) } else { (4, 5, 7) };
        let head = canonical_coord(state, owner, bird.head());
        grid[channels.0][head.y as usize][head.x as usize] = 1.0;
        for segment in bird.body.iter().skip(1) {
            let view = canonical_coord(state, owner, *segment);
            grid[channels.1][view.y as usize][view.x as usize] = 1.0;
        }
        for segment in bird.body.iter().copied() {
            if segment_has_support(state, bird.id, segment) {
                let view = canonical_coord(state, owner, segment);
                grid[channels.2][view.y as usize][view.x as usize] = 1.0;
            }
        }
    }

    EncodedPosition {
        grid,
        scalars: scalar_features(state, owner),
    }
}

pub fn encode_training_row(state: &GameState, owner: usize, final_scores: [i32; 2]) -> TrainingRow {
    let encoded = encode_position(state, owner);
    let score_diff = (final_scores[owner] - final_scores[1 - owner]) as f32;
    TrainingRow {
        grid: encoded.grid,
        scalars: encoded.scalars,
        value: (score_diff / VALUE_SCALE).tanh(),
        weight: 1.0,
    }
}

fn scalar_features(state: &GameState, owner: usize) -> Vec<f32> {
    let body_scores = state.body_scores();
    let live_diff = live_bird_count(state, owner) as f32 - live_bird_count(state, 1 - owner) as f32;
    let breakpoint_diff =
        breakpoint_count(state, owner) as f32 - breakpoint_count(state, 1 - owner) as f32;
    let mobility_diff = mobility_count(state, owner) as f32 - mobility_count(state, 1 - owner) as f32;

    vec![
        (state.turn as f32 / 200.0).clamp(0.0, 1.0),
        ((body_scores[owner] - body_scores[1 - owner]) as f32 / 32.0).clamp(-1.0, 1.0),
        ((state.losses[1 - owner] - state.losses[owner]) as f32 / 32.0).clamp(-1.0, 1.0),
        (live_diff / 4.0).clamp(-1.0, 1.0),
        (breakpoint_diff / 4.0).clamp(-1.0, 1.0),
        (mobility_diff / 16.0).clamp(-1.0, 1.0),
    ]
}

fn canonical_coord(state: &GameState, owner: usize, coord: Coord) -> Coord {
    if owner == 0 {
        coord
    } else {
        state.grid.opposite(coord)
    }
}

fn live_bird_count(state: &GameState, owner: usize) -> usize {
    state.birds_for_player(owner).filter(|bird| bird.alive).count()
}

fn breakpoint_count(state: &GameState, owner: usize) -> usize {
    state
        .birds_for_player(owner)
        .filter(|bird| bird.alive && bird.length() >= 4)
        .count()
}

fn mobility_count(state: &GameState, owner: usize) -> usize {
    state
        .birds_for_player(owner)
        .filter(|bird| bird.alive)
        .map(|bird| state.legal_commands_for_bird(bird.id).len())
        .sum()
}

fn segment_has_support(state: &GameState, bird_id: i32, coord: Coord) -> bool {
    let below = coord.add(0, 1);
    if state.grid.get(below) == Some(TileType::Wall) || state.grid.apples.contains(&below) {
        return true;
    }
    state
        .birds
        .iter()
        .filter(|bird| bird.alive && bird.id != bird_id)
        .any(|bird| bird.body.contains(&below))
}

#[cfg(test)]
mod tests {
    use snakebot_engine::{Direction, Grid};

    use super::{encode_position, GRID_CHANNELS, SCALAR_FEATURES};
    use snakebot_engine::{Coord, GameState, TileType};

    #[test]
    fn encodes_expected_channel_counts() {
        let mut grid = Grid::new(4, 4);
        grid.set(Coord::new(0, 3), TileType::Wall);
        grid.apples.push(Coord::new(2, 2));
        let mut state = GameState::new(grid);
        state.add_bird(
            0,
            0,
            vec![Coord::new(1, 1), Coord::new(1, 2), Coord::new(1, 3)],
            Some(Direction::North),
        );
        let encoded = encode_position(&state, 0);
        assert_eq!(encoded.grid.len(), GRID_CHANNELS);
        assert_eq!(encoded.scalars.len(), SCALAR_FEATURES);
    }

    #[test]
    fn mirrors_player_one_view() {
        let grid = Grid::new(5, 4);
        let mut state = GameState::new(grid);
        state.add_bird(
            1,
            1,
            vec![Coord::new(4, 1), Coord::new(4, 2), Coord::new(4, 3)],
            Some(Direction::North),
        );
        let encoded = encode_position(&state, 1);
        assert_eq!(encoded.grid[2][1][0], 1.0);
    }
}
