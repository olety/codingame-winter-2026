use std::collections::BTreeMap;
use std::env;
use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use snakebot_engine::{
    initial_state_from_seed, BirdCommand, Coord, Direction, GameState, OracleState, PlayerAction,
    TileType,
};

#[derive(Clone, Copy)]
struct DiffConfig {
    league: i32,
    seeds: usize,
    max_turns: usize,
    init_source: InitSource,
}

#[derive(Clone, Copy, Eq, PartialEq)]
enum InitSource {
    Java,
    Rust,
}

fn main() -> Result<(), Box<dyn Error>> {
    let config = parse_args()?;
    let repo_root = repo_root();
    ensure_java_oracle_compiled(&repo_root)?;

    for seed in 1..=config.seeds {
        run_seed(
            &repo_root,
            seed as i64,
            config.league,
            config.max_turns,
            config.init_source,
        )?;
        println!("seed {seed}: ok");
    }
    println!(
        "java diff passed for {} seed(s) at league {} over up to {} turns",
        config.seeds, config.league, config.max_turns
    );
    Ok(())
}

fn parse_args() -> Result<DiffConfig, Box<dyn Error>> {
    let mut config = DiffConfig {
        league: 4,
        seeds: 10,
        max_turns: 50,
        init_source: InitSource::Java,
    };
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--league" => {
                config.league = args.next().ok_or("missing value for --league")?.parse()?;
            }
            "--seeds" => {
                config.seeds = args.next().ok_or("missing value for --seeds")?.parse()?;
            }
            "--turns" => {
                config.max_turns = args.next().ok_or("missing value for --turns")?.parse()?;
            }
            "--init-source" => {
                config.init_source = match args.next().ok_or("missing value for --init-source")?.as_str() {
                    "java" => InitSource::Java,
                    "rust" => InitSource::Rust,
                    other => return Err(format!("unknown init source: {other}").into()),
                };
            }
            _ => return Err(format!("unknown arg: {arg}").into()),
        }
    }
    Ok(config)
}

fn repo_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(Path::parent)
        .expect("workspace root")
        .to_path_buf()
}

fn ensure_java_oracle_compiled(repo_root: &Path) -> Result<(), Box<dyn Error>> {
    let status = Command::new("mvn")
        .current_dir(repo_root)
        .args([
            "-q",
            "-DskipTests",
            "test-compile",
            "dependency:build-classpath",
            "-Dmdep.outputFile=cp.txt",
        ])
        .status()?;
    if !status.success() {
        return Err("maven build failed".into());
    }
    Ok(())
}

fn run_seed(
    repo_root: &Path,
    seed: i64,
    league: i32,
    max_turns: usize,
    init_source: InitSource,
) -> Result<(), Box<dyn Error>> {
    let initial_states = run_java_oracle(repo_root, seed, league, "")?;
    let initial_oracle = initial_states
        .first()
        .ok_or("oracle did not emit an initial state")?;
    let mut rust_state = match init_source {
        InitSource::Java => initial_oracle.to_game_state(),
        InitSource::Rust => initial_state_from_seed(seed, league),
    };
    let mut script_lines = Vec::new();

    compare_states(seed, 0, "initial", &rust_state, initial_oracle)?;

    for turn in 1..=max_turns {
        if rust_state.is_game_over() {
            break;
        }
        let p0 = choose_scripted_action(&rust_state, 0, seed, turn as i64);
        let p1 = choose_scripted_action(&rust_state, 1, seed.wrapping_mul(31), turn as i64);
        let script_line = format!("{}|{}", render_action(&p0), render_action(&p1));
        script_lines.push(script_line.clone());

        rust_state.step(&p0, &p1);
        let oracle = run_java_oracle(repo_root, seed, league, &script_lines.join("\n"))?;
        let oracle_state = oracle
            .last()
            .ok_or("oracle did not emit a post-turn state")?;
        compare_states(seed, turn, &script_line, &rust_state, oracle_state)?;
    }
    Ok(())
}

fn run_java_oracle(
    repo_root: &Path,
    seed: i64,
    league: i32,
    script: &str,
) -> Result<Vec<OracleState>, Box<dyn Error>> {
    let classpath = format!(
        "target/classes:target/test-classes:{}",
        fs::read_to_string(repo_root.join("cp.txt"))?.trim()
    );
    let output = Command::new("java")
        .current_dir(repo_root)
        .args([
            "-cp",
            &classpath,
            "com.codingame.game.OracleCli",
            &seed.to_string(),
            &league.to_string(),
        ])
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .spawn()
        .and_then(|mut child| {
            if let Some(stdin) = child.stdin.as_mut() {
                use std::io::Write;
                if !script.is_empty() {
                    stdin.write_all(script.as_bytes())?;
                    stdin.write_all(b"\n")?;
                }
            }
            child.wait_with_output()
        })?;
    if !output.status.success() {
        return Err(format!(
            "java oracle failed: {}",
            String::from_utf8_lossy(&output.stderr)
        )
        .into());
    }

    let states = String::from_utf8(output.stdout)?
        .lines()
        .filter(|line| !line.trim().is_empty())
        .map(serde_json::from_str)
        .collect::<Result<Vec<OracleState>, _>>()?;
    Ok(states)
}

fn choose_scripted_action(state: &GameState, owner: usize, seed: i64, turn: i64) -> PlayerAction {
    let mut action = PlayerAction::default();
    for bird in state.birds_for_player(owner).filter(|bird| bird.alive) {
        let legal = state.legal_commands_for_bird(bird.id);
        if legal.is_empty() {
            continue;
        }
        let idx = deterministic_index(seed, turn, bird.id, legal.len());
        let command = legal[idx];
        if let BirdCommand::Turn(direction) = command {
            action
                .per_bird
                .insert(bird.id, BirdCommand::Turn(direction));
        }
    }
    action
}

fn deterministic_index(seed: i64, turn: i64, bird_id: i32, len: usize) -> usize {
    let mut x = (seed as u64)
        .wrapping_mul(0x9E37_79B9_7F4A_7C15)
        .wrapping_add(turn as u64)
        .wrapping_mul(0xBF58_476D_1CE4_E5B9)
        .wrapping_add((bird_id as i64 as u64).wrapping_mul(0x94D0_49BB_1331_11EB));
    x ^= x >> 30;
    x = x.wrapping_mul(0xBF58_476D_1CE4_E5B9);
    x ^= x >> 27;
    x = x.wrapping_mul(0x94D0_49BB_1331_11EB);
    x ^= x >> 31;
    (x as usize) % len
}

fn render_action(action: &PlayerAction) -> String {
    let mut parts = action
        .per_bird
        .iter()
        .map(|(bird_id, command)| {
            let alias = match command {
                BirdCommand::Keep => "K",
                BirdCommand::Turn(Direction::North) => "N",
                BirdCommand::Turn(Direction::East) => "E",
                BirdCommand::Turn(Direction::South) => "S",
                BirdCommand::Turn(Direction::West) => "W",
                BirdCommand::Turn(Direction::Unset) => "K",
            };
            format!("{bird_id}:{alias}")
        })
        .collect::<Vec<_>>();
    parts.sort();
    parts.join(",")
}

fn compare_states(
    seed: i64,
    turn: usize,
    script_line: &str,
    rust_state: &GameState,
    oracle: &OracleState,
) -> Result<(), Box<dyn Error>> {
    if rust_state.grid.width != oracle.width || rust_state.grid.height != oracle.height {
        return Err(mismatch(
            seed,
            turn,
            script_line,
            "grid dimensions",
            rust_state,
            oracle,
        )
        .into());
    }
    if coord_vec(
        rust_state
            .grid
            .coords()
            .into_iter()
            .filter(|c| rust_state.grid.get(*c) == Some(TileType::Wall))
            .collect(),
    ) != oracle.walls
    {
        return Err(mismatch(seed, turn, script_line, "walls", rust_state, oracle).into());
    }
    if coord_vec(rust_state.grid.spawns.clone()) != oracle.spawns {
        return Err(mismatch(seed, turn, script_line, "spawns", rust_state, oracle).into());
    }
    if coord_vec(rust_state.grid.apples.clone()) != oracle.apples {
        return Err(mismatch(seed, turn, script_line, "apples", rust_state, oracle).into());
    }
    if rust_state.losses.to_vec() != oracle.losses {
        return Err(mismatch(seed, turn, script_line, "losses", rust_state, oracle).into());
    }
    if rust_state.is_game_over() != oracle.game_over {
        return Err(mismatch(seed, turn, script_line, "gameOver", rust_state, oracle).into());
    }

    let rust_birds = rust_state
        .birds
        .iter()
        .map(|bird| {
            (
                bird.id,
                (
                    bird.owner,
                    bird.alive,
                    bird.body
                        .iter()
                        .map(|coord| [coord.x, coord.y])
                        .collect::<Vec<_>>(),
                ),
            )
        })
        .collect::<BTreeMap<_, _>>();
    let oracle_birds = oracle
        .birds
        .iter()
        .map(|bird| (bird.id, (bird.owner, bird.alive, bird.body.clone())))
        .collect::<BTreeMap<_, _>>();
    if rust_birds != oracle_birds {
        return Err(mismatch(seed, turn, script_line, "birds", rust_state, oracle).into());
    }
    Ok(())
}

fn coord_vec(mut coords: Vec<Coord>) -> Vec<[i32; 2]> {
    coords.sort();
    coords.into_iter().map(|coord| [coord.x, coord.y]).collect()
}

fn mismatch(
    seed: i64,
    turn: usize,
    script_line: &str,
    field: &str,
    rust_state: &GameState,
    oracle: &OracleState,
) -> String {
    let rust_json = serde_json::to_string_pretty(&serialize_rust_state(rust_state)).unwrap();
    let oracle_json = serde_json::to_string_pretty(oracle).unwrap();
    format!(
        "mismatch on {field} for seed={seed} turn={turn} action='{script_line}'\nRUST:\n{rust_json}\nJAVA:\n{oracle_json}"
    )
}

fn serialize_rust_state(state: &GameState) -> serde_json::Value {
    serde_json::json!({
        "width": state.grid.width,
        "height": state.grid.height,
        "walls": coord_vec(state.grid.coords().into_iter().filter(|c| state.grid.get(*c) == Some(TileType::Wall)).collect()),
        "spawns": coord_vec(state.grid.spawns.clone()),
        "apples": coord_vec(state.grid.apples.clone()),
        "losses": state.losses,
        "gameOver": state.is_game_over(),
        "birds": state.birds.iter().map(|bird| serde_json::json!({
            "id": bird.id,
            "owner": bird.owner,
            "alive": bird.alive,
            "body": bird.body.iter().map(|coord| [coord.x, coord.y]).collect::<Vec<_>>(),
        })).collect::<Vec<_>>(),
    })
}
