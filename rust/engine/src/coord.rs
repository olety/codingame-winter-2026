use std::cmp::Ordering;

use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, Serialize, Deserialize)]
pub struct Coord {
    pub x: i32,
    pub y: i32,
}

impl Coord {
    pub const fn new(x: i32, y: i32) -> Self {
        Self { x, y }
    }

    pub const fn add(self, dx: i32, dy: i32) -> Self {
        Self::new(self.x + dx, self.y + dy)
    }

    pub const fn add_coord(self, other: Coord) -> Self {
        Self::new(self.x + other.x, self.y + other.y)
    }

    pub fn manhattan_to(self, other: Coord) -> i32 {
        (self.x - other.x).abs() + (self.y - other.y).abs()
    }

    pub fn to_int_string(self) -> String {
        format!("{} {}", self.x, self.y)
    }
}

impl Ord for Coord {
    fn cmp(&self, other: &Self) -> Ordering {
        match self.x.cmp(&other.x) {
            Ordering::Equal => self.y.cmp(&other.y),
            ord => ord,
        }
    }
}

impl PartialOrd for Coord {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
