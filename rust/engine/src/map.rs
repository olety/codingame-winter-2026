use std::collections::{BTreeSet, VecDeque};

use serde::{Deserialize, Serialize};

use crate::{Coord, Direction};

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum TileType {
    Empty,
    Wall,
}

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub struct Grid {
    pub width: i32,
    pub height: i32,
    pub tiles: Vec<TileType>,
    pub spawns: Vec<Coord>,
    pub apples: Vec<Coord>,
}

impl Grid {
    pub const ADJACENCY: [Coord; 4] = [
        Direction::North.delta(),
        Direction::East.delta(),
        Direction::South.delta(),
        Direction::West.delta(),
    ];

    pub const ADJACENCY_8: [Coord; 8] = [
        Direction::North.delta(),
        Direction::East.delta(),
        Direction::South.delta(),
        Direction::West.delta(),
        Coord::new(-1, -1),
        Coord::new(1, 1),
        Coord::new(1, -1),
        Coord::new(-1, 1),
    ];

    pub fn new(width: i32, height: i32) -> Self {
        let len = (width * height) as usize;
        Self {
            width,
            height,
            tiles: vec![TileType::Empty; len],
            spawns: Vec::new(),
            apples: Vec::new(),
        }
    }

    pub fn is_valid(&self, coord: Coord) -> bool {
        coord.x >= 0 && coord.x < self.width && coord.y >= 0 && coord.y < self.height
    }

    pub fn index(&self, coord: Coord) -> Option<usize> {
        self.is_valid(coord)
            .then_some((coord.y * self.width + coord.x) as usize)
    }

    pub fn get(&self, coord: Coord) -> Option<TileType> {
        self.index(coord).map(|idx| self.tiles[idx])
    }

    pub fn set(&mut self, coord: Coord, tile: TileType) {
        if let Some(idx) = self.index(coord) {
            self.tiles[idx] = tile;
        }
    }

    pub fn coords(&self) -> Vec<Coord> {
        let mut coords = Vec::with_capacity(self.tiles.len());
        for y in 0..self.height {
            for x in 0..self.width {
                coords.push(Coord::new(x, y));
            }
        }
        coords
    }

    pub fn neighbours(&self, coord: Coord, adjacency: &[Coord]) -> Vec<Coord> {
        adjacency
            .iter()
            .map(|delta| coord.add_coord(*delta))
            .filter(|next| self.is_valid(*next))
            .collect()
    }

    pub fn opposite(&self, coord: Coord) -> Coord {
        Coord::new(self.width - coord.x - 1, coord.y)
    }

    pub fn sorted_unique_apples(&mut self) {
        let mut set = BTreeSet::new();
        for apple in self.apples.drain(..) {
            set.insert(apple);
        }
        self.apples = set.into_iter().collect();
    }

    pub fn detect_air_pockets(&self) -> Vec<BTreeSet<Coord>> {
        self.detect_regions(
            self.coords()
                .into_iter()
                .filter(|coord| self.get(*coord) == Some(TileType::Empty))
                .collect(),
        )
    }

    pub fn detect_spawn_islands(&self) -> Vec<BTreeSet<Coord>> {
        let mut result = Vec::new();
        let candidates = self.spawns.iter().copied().collect::<BTreeSet<_>>();
        let mut seen = BTreeSet::new();
        for start in self.spawns.iter().copied() {
            if seen.contains(&start) {
                continue;
            }
            let mut current = BTreeSet::new();
            let mut queue = VecDeque::from([start]);
            seen.insert(start);
            while let Some(coord) = queue.pop_front() {
                current.insert(coord);
                for next in self.neighbours(coord, &Self::ADJACENCY) {
                    if !candidates.contains(&next) || seen.contains(&next) {
                        continue;
                    }
                    seen.insert(next);
                    queue.push_back(next);
                }
            }
            result.push(current);
        }
        result
    }

    fn detect_regions(&self, candidates: BTreeSet<Coord>) -> Vec<BTreeSet<Coord>> {
        let mut result = Vec::new();
        let mut seen = BTreeSet::new();
        for start in candidates.iter().copied() {
            if seen.contains(&start) {
                continue;
            }
            let mut current = BTreeSet::new();
            let mut queue = VecDeque::from([start]);
            seen.insert(start);
            while let Some(coord) = queue.pop_front() {
                current.insert(coord);
                for next in self.neighbours(coord, &Self::ADJACENCY) {
                    if !candidates.contains(&next) || seen.contains(&next) {
                        continue;
                    }
                    seen.insert(next);
                    queue.push_back(next);
                }
            }
            result.push(current);
        }
        result
    }

    pub fn detect_lowest_island(&self) -> Vec<Coord> {
        let start = Coord::new(0, self.height - 1);
        if self.get(start) != Some(TileType::Wall) {
            return Vec::new();
        }

        let mut result = Vec::new();
        let mut seen = BTreeSet::new();
        let mut queue = VecDeque::from([start]);
        seen.insert(start);
        while let Some(coord) = queue.pop_front() {
            result.push(coord);
            for next in self.neighbours(coord, &Self::ADJACENCY) {
                if seen.contains(&next) || self.get(next) != Some(TileType::Wall) {
                    continue;
                }
                seen.insert(next);
                queue.push_back(next);
            }
        }
        result
    }

    pub fn get_free_above(&self, coord: Coord, count: i32) -> Vec<Coord> {
        let mut result = Vec::new();
        for step in 1..=count {
            let above = Coord::new(coord.x, coord.y - step);
            if self.get(above) == Some(TileType::Empty) {
                result.push(above);
            } else {
                break;
            }
        }
        result
    }
}
