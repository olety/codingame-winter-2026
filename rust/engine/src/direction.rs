use serde::{Deserialize, Serialize};

use crate::Coord;

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum Direction {
    North,
    East,
    South,
    West,
    Unset,
}

impl Direction {
    pub const ALL: [Direction; 4] = [
        Direction::North,
        Direction::East,
        Direction::South,
        Direction::West,
    ];

    pub const fn delta(self) -> Coord {
        match self {
            Direction::North => Coord::new(0, -1),
            Direction::East => Coord::new(1, 0),
            Direction::South => Coord::new(0, 1),
            Direction::West => Coord::new(-1, 0),
            Direction::Unset => Coord::new(0, 0),
        }
    }

    pub const fn alias(self) -> &'static str {
        match self {
            Direction::North => "N",
            Direction::East => "E",
            Direction::South => "S",
            Direction::West => "W",
            Direction::Unset => "X",
        }
    }

    pub const fn opposite(self) -> Direction {
        match self {
            Direction::North => Direction::South,
            Direction::East => Direction::West,
            Direction::South => Direction::North,
            Direction::West => Direction::East,
            Direction::Unset => Direction::Unset,
        }
    }

    pub const fn from_coord(coord: Coord) -> Direction {
        match (coord.x, coord.y) {
            (0, -1) => Direction::North,
            (1, 0) => Direction::East,
            (0, 1) => Direction::South,
            (-1, 0) => Direction::West,
            _ => Direction::Unset,
        }
    }

    pub fn from_command(command: &str) -> Option<Direction> {
        match command {
            "UP" => Some(Direction::North),
            "RIGHT" => Some(Direction::East),
            "DOWN" => Some(Direction::South),
            "LEFT" => Some(Direction::West),
            _ => None,
        }
    }
}
