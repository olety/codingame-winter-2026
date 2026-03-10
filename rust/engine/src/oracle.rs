use std::collections::VecDeque;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::{BirdState, Coord, GameState, Grid, TileType};

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, Eq)]
pub struct OracleBird {
    pub id: i32,
    pub owner: usize,
    pub alive: bool,
    pub body: Vec<[i32; 2]>,
}

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, Eq)]
pub struct OracleState {
    pub width: i32,
    pub height: i32,
    pub walls: Vec<[i32; 2]>,
    pub spawns: Vec<[i32; 2]>,
    pub apples: Vec<[i32; 2]>,
    pub losses: Vec<i32>,
    #[serde(rename = "gameOver")]
    pub game_over: bool,
    pub birds: Vec<OracleBird>,
}

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, Eq)]
pub struct OracleDumpRecord {
    pub seed: i64,
    #[serde(rename = "leagueLevel")]
    pub league_level: i32,
    pub state: OracleState,
}

impl OracleState {
    pub fn to_game_state(&self) -> GameState {
        let mut grid = Grid::new(self.width, self.height);
        for wall in &self.walls {
            grid.set(Coord::new(wall[0], wall[1]), TileType::Wall);
        }
        grid.spawns = self
            .spawns
            .iter()
            .map(|coord| Coord::new(coord[0], coord[1]))
            .collect();
        grid.apples = self
            .apples
            .iter()
            .map(|coord| Coord::new(coord[0], coord[1]))
            .collect();
        let mut state = GameState::new(grid);
        state.losses = self.losses.clone().try_into().unwrap_or([0, 0]);
        state.birds = self
            .birds
            .iter()
            .map(|bird| BirdState {
                id: bird.id,
                owner: bird.owner,
                alive: bird.alive,
                direction: None,
                body: bird
                    .body
                    .iter()
                    .map(|coord| Coord::new(coord[0], coord[1]))
                    .collect::<VecDeque<_>>(),
            })
            .collect();
        state.birds.sort_by_key(|bird| bird.id);
        state
    }

    pub fn from_game_state(state: &GameState) -> Self {
        let mut walls = state
            .grid
            .coords()
            .into_iter()
            .filter(|coord| state.grid.get(*coord) == Some(TileType::Wall))
            .map(|coord| [coord.x, coord.y])
            .collect::<Vec<_>>();
        walls.sort();
        let mut spawns = state
            .grid
            .spawns
            .iter()
            .map(|coord| [coord.x, coord.y])
            .collect::<Vec<_>>();
        spawns.sort();
        let mut apples = state
            .grid
            .apples
            .iter()
            .map(|coord| [coord.x, coord.y])
            .collect::<Vec<_>>();
        apples.sort();
        let mut birds = state
            .birds
            .iter()
            .map(|bird| OracleBird {
                id: bird.id,
                owner: bird.owner,
                alive: bird.alive,
                body: bird.body.iter().map(|coord| [coord.x, coord.y]).collect(),
            })
            .collect::<Vec<_>>();
        birds.sort_by_key(|bird| bird.id);

        Self {
            width: state.grid.width,
            height: state.grid.height,
            walls,
            spawns,
            apples,
            losses: state.losses.to_vec(),
            game_over: state.is_game_over(),
            birds,
        }
    }
}

pub fn parse_dump_line(line: &str) -> serde_json::Result<OracleDumpRecord> {
    serde_json::from_str(line)
}

pub fn load_dump_records(
    path: impl AsRef<Path>,
) -> Result<Vec<OracleDumpRecord>, Box<dyn std::error::Error>> {
    let reader = BufReader::new(File::open(path)?);
    let mut records = Vec::new();
    for line in reader.lines() {
        let line = line?;
        if line.trim().is_empty() {
            continue;
        }
        records.push(parse_dump_line(&line)?);
    }
    Ok(records)
}

#[cfg(test)]
mod tests {
    use super::{parse_dump_line, OracleState};
    use crate::initial_state_from_seed;

    #[test]
    fn converts_between_game_state_and_oracle_state() {
        let state = initial_state_from_seed(1, 4);
        let oracle = OracleState::from_game_state(&state);
        let roundtrip = oracle.to_game_state();
        assert_eq!(OracleState::from_game_state(&roundtrip), oracle);
    }

    #[test]
    fn parses_dump_record() {
        let line = r#"{"seed":7,"leagueLevel":4,"state":{"width":2,"height":2,"walls":[],"spawns":[],"apples":[],"losses":[0,0],"gameOver":true,"birds":[]}}"#;
        let record = parse_dump_line(line).expect("record");
        assert_eq!(record.seed, 7);
        assert_eq!(record.league_level, 4);
        assert!(record.state.game_over);
    }
}
