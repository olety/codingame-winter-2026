use std::collections::BTreeSet;
use std::env;

use snakebot_engine::{java_random::JavaRandom, Coord, Grid, TileType};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = env::args().skip(1);
    let seed: i64 = args.next().ok_or("missing seed")?.parse()?;
    let league_level: i32 = args.next().ok_or("missing league level")?.parse()?;

    let mut random = JavaRandom::new(seed);
    let skew = match league_level {
        1 => 2.0,
        2 => 1.0,
        3 => 0.8,
        _ => 0.3,
    };

    let rand = random.next_double();
    let height = snakebot_engine::mapgen::MIN_GRID_HEIGHT
        + (f64::powf(rand, skew)
            * f64::from(
                snakebot_engine::mapgen::MAX_GRID_HEIGHT - snakebot_engine::mapgen::MIN_GRID_HEIGHT,
            ))
        .round() as i32;
    let mut width = ((height as f32) * snakebot_engine::mapgen::ASPECT_RATIO).round() as i32;
    if width % 2 != 0 {
        width += 1;
    }
    let mut grid = Grid::new(width, height);
    let b = 5.0 + random.next_double() * 10.0;

    for x in 0..width {
        grid.set(Coord::new(x, height - 1), TileType::Wall);
    }
    for y in (0..(height - 1)).rev() {
        let y_norm = f64::from(height - 1 - y) / f64::from(height - 1);
        let block_chance = 1.0 / (y_norm + 0.1) / b;
        for x in 0..width {
            if random.next_double() < block_chance {
                grid.set(Coord::new(x, y), TileType::Wall);
            }
        }
    }
    dump("base", &grid, None);

    for coord in grid.coords() {
        let opp = grid.opposite(coord);
        if let Some(tile) = grid.get(coord) {
            grid.set(opp, tile);
        }
    }
    dump("symmetry", &grid, None);

    for island in grid.detect_air_pockets() {
        if island.len() < 10 {
            for coord in island {
                grid.set(coord, TileType::Wall);
            }
        }
    }
    dump("fill_islands", &grid, None);

    let coords = grid.coords();
    let mut something_destroyed = true;
    while something_destroyed {
        something_destroyed = false;
        for coord in coords.iter().copied() {
            if grid.get(coord) != Some(TileType::Empty) {
                continue;
            }
            let neighbour_walls: Vec<_> = grid
                .neighbours(coord, &Grid::ADJACENCY)
                .into_iter()
                .filter(|next| grid.get(*next) == Some(TileType::Wall))
                .collect();
            if neighbour_walls.len() < 3 {
                continue;
            }
            let mut destroyable: Vec<_> = neighbour_walls
                .into_iter()
                .filter(|next| next.y <= coord.y)
                .collect();
            random.shuffle(&mut destroyable);
            if let Some(target) = destroyable.first().copied() {
                grid.set(target, TileType::Empty);
                grid.set(grid.opposite(target), TileType::Empty);
                something_destroyed = true;
            }
        }
    }
    dump("destroy_walls", &grid, None);

    let island = grid.detect_lowest_island();
    let island_set: BTreeSet<_> = island.iter().copied().collect();
    let mut lower_by = 0;
    let mut can_lower = true;
    while can_lower {
        for x in 0..width {
            let coord = Coord::new(x, height - 1 - (lower_by + 1));
            if !island_set.contains(&coord) {
                can_lower = false;
                break;
            }
        }
        if can_lower {
            lower_by += 1;
        }
    }
    if lower_by >= 2 {
        lower_by = random.next_int_range(2, lower_by + 1);
    }
    for coord in island.iter().copied() {
        grid.set(coord, TileType::Empty);
        grid.set(grid.opposite(coord), TileType::Empty);
    }
    for coord in island.iter().copied() {
        let lowered = Coord::new(coord.x, coord.y + lower_by);
        if grid.is_valid(lowered) {
            grid.set(lowered, TileType::Wall);
            grid.set(grid.opposite(lowered), TileType::Wall);
        }
    }
    dump("sink_lowest", &grid, Some(lower_by));

    for y in 0..height {
        for x in 0..(width / 2) {
            let coord = Coord::new(x, y);
            if grid.get(coord) == Some(TileType::Empty) && random.next_double() < 0.025 {
                grid.apples.push(coord);
                grid.apples.push(grid.opposite(coord));
            }
        }
    }
    dump("random_apples", &grid, Some(lower_by));

    for coord in coords.iter().copied() {
        if grid.get(coord) != Some(TileType::Wall) {
            continue;
        }
        let neighbour_wall_count = grid
            .neighbours(coord, &Grid::ADJACENCY_8)
            .into_iter()
            .filter(|next| grid.get(*next) == Some(TileType::Wall))
            .count();
        if neighbour_wall_count == 0 {
            grid.set(coord, TileType::Empty);
            let opp = grid.opposite(coord);
            grid.set(opp, TileType::Empty);
            grid.apples.push(coord);
            grid.apples.push(opp);
        }
    }
    dump("lone_walls", &grid, Some(lower_by));

    let mut potential_spawns: Vec<_> = coords
        .iter()
        .copied()
        .filter(|coord| {
            grid.get(*coord) == Some(TileType::Wall)
                && grid
                    .get_free_above(*coord, snakebot_engine::mapgen::SPAWN_HEIGHT)
                    .len()
                    == snakebot_engine::mapgen::SPAWN_HEIGHT as usize
        })
        .collect();
    random.shuffle(&mut potential_spawns);

    let mut desired_spawns = snakebot_engine::mapgen::DESIRED_SPAWNS;
    if height <= 15 {
        desired_spawns -= 1;
    }
    if height <= 10 {
        desired_spawns -= 1;
    }
    while desired_spawns > 0 && !potential_spawns.is_empty() {
        let spawn = potential_spawns.remove(0);
        let spawn_loc = grid.get_free_above(spawn, snakebot_engine::mapgen::SPAWN_HEIGHT);
        let mut too_close = false;
        for coord in spawn_loc.iter().copied() {
            if coord.x == width / 2 - 1 || coord.x == width / 2 {
                too_close = true;
                break;
            }
            for neighbour in grid.neighbours(coord, &Grid::ADJACENCY_8) {
                if grid.spawns.contains(&neighbour)
                    || grid.spawns.contains(&grid.opposite(neighbour))
                {
                    too_close = true;
                    break;
                }
            }
            if too_close {
                break;
            }
        }
        if too_close {
            continue;
        }

        for coord in spawn_loc {
            grid.spawns.push(coord);
            let opp = grid.opposite(coord);
            grid.apples.retain(|apple| *apple != coord && *apple != opp);
        }
        desired_spawns -= 1;
    }
    grid.sorted_unique_apples();
    dump("spawns", &grid, Some(lower_by));

    Ok(())
}

fn dump(stage: &str, grid: &Grid, lower_by: Option<i32>) {
    let mut payload = serde_json::Map::new();
    payload.insert("stage".into(), serde_json::json!(stage));
    if let Some(lower_by) = lower_by {
        payload.insert("lowerBy".into(), serde_json::json!(lower_by));
    }
    payload.insert("width".into(), serde_json::json!(grid.width));
    payload.insert("height".into(), serde_json::json!(grid.height));
    payload.insert(
        "walls".into(),
        serde_json::json!(coord_vec(
            grid.coords()
                .into_iter()
                .filter(|coord| grid.get(*coord) == Some(TileType::Wall))
                .collect()
        )),
    );
    payload.insert(
        "apples".into(),
        serde_json::json!(coord_vec(grid.apples.clone())),
    );
    payload.insert(
        "spawns".into(),
        serde_json::json!(coord_vec(grid.spawns.clone())),
    );
    println!("{}", serde_json::Value::Object(payload));
}

fn coord_vec(mut coords: Vec<Coord>) -> Vec<[i32; 2]> {
    coords.sort();
    coords.into_iter().map(|coord| [coord.x, coord.y]).collect()
}
