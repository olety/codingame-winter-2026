use std::collections::BTreeMap;
use std::env;
use std::error::Error;
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use rayon::prelude::*;
use snakebot_engine::{
    initial_state_from_seed, BirdCommand, Direction, GameState, PlayerAction, TileType,
};

// ---------------------------------------------------------------------------
// Config & result types
// ---------------------------------------------------------------------------

struct ExtArenaConfig {
    agent_a: String,
    agent_b: String,
    seeds: Vec<i64>,
    league: i32,
    jobs: usize,
    max_turns: i32,
    timeout_first_ms: u64,
    timeout_later_ms: u64,
    verbose: bool,
}

#[derive(Clone, Debug)]
struct MatchResult {
    seed: i64,
    bot_a_is_player_zero: bool,
    body_diff_a: i32,
    winner_a: Option<bool>,
    turns: i32,
    error: Option<String>,
}

// ---------------------------------------------------------------------------
// Protocol formatting — generate the strings bots expect
// ---------------------------------------------------------------------------

fn format_init(state: &GameState, player_index: usize) -> String {
    let mut out = String::new();

    // player_index
    out.push_str(&format!("{}\n", player_index));

    // width, height
    out.push_str(&format!("{}\n", state.grid.width));
    out.push_str(&format!("{}\n", state.grid.height));

    // grid rows
    for y in 0..state.grid.height {
        for x in 0..state.grid.width {
            let coord = snakebot_engine::Coord::new(x, y);
            match state.grid.get(coord) {
                Some(TileType::Wall) => out.push('#'),
                _ => out.push('.'),
            }
        }
        out.push('\n');
    }

    // Collect bird IDs per player (sorted ascending)
    let mut my_ids: Vec<i32> = state
        .birds_for_player(player_index)
        .map(|b| b.id)
        .collect();
    my_ids.sort();
    let mut opp_ids: Vec<i32> = state
        .birds_for_player(1 - player_index)
        .map(|b| b.id)
        .collect();
    opp_ids.sort();

    // birds_per_player (assume symmetric)
    out.push_str(&format!("{}\n", my_ids.len()));

    // my bird IDs
    for id in &my_ids {
        out.push_str(&format!("{}\n", id));
    }
    // opp bird IDs
    for id in &opp_ids {
        out.push_str(&format!("{}\n", id));
    }

    out
}

fn format_frame(state: &GameState) -> String {
    let mut out = String::new();

    // apple_count + apple lines
    out.push_str(&format!("{}\n", state.grid.apples.len()));
    for apple in &state.grid.apples {
        out.push_str(&format!("{} {}\n", apple.x, apple.y));
    }

    // live birds
    let live: Vec<_> = state.birds.iter().filter(|b| b.alive).collect();
    out.push_str(&format!("{}\n", live.len()));
    for bird in &live {
        let body_str: Vec<String> = bird.body.iter().map(|c| format!("{},{}", c.x, c.y)).collect();
        out.push_str(&format!("{} {}\n", bird.id, body_str.join(":")));
    }

    out
}

// ---------------------------------------------------------------------------
// Parse bot output into PlayerAction
// ---------------------------------------------------------------------------

fn parse_bot_output(line: &str) -> PlayerAction {
    let trimmed = line.trim();
    if trimmed.is_empty() || trimmed.eq_ignore_ascii_case("WAIT") {
        return PlayerAction::default();
    }

    let mut action = PlayerAction {
        per_bird: BTreeMap::new(),
    };

    for part in trimmed.split(';') {
        let part = part.trim();
        if part.is_empty() || part.eq_ignore_ascii_case("WAIT") {
            continue;
        }
        let tokens: Vec<&str> = part.split_whitespace().collect();
        if tokens.len() >= 2 {
            if let Ok(bird_id) = tokens[0].parse::<i32>() {
                if let Some(dir) = Direction::from_command(tokens[1]) {
                    action.per_bird.insert(bird_id, BirdCommand::Turn(dir));
                }
            }
        }
    }

    action
}

// ---------------------------------------------------------------------------
// Bot process wrapper
// ---------------------------------------------------------------------------

struct BotProcess {
    stdin: std::process::ChildStdin,
    reader: BufReader<std::process::ChildStdout>,
    alive: bool,
}

impl BotProcess {
    fn spawn(path: &str) -> Result<Self, Box<dyn Error>> {
        let child = Command::new(path)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()?;
        let stdin = child.stdin.unwrap();
        let stdout = child.stdout.unwrap();
        Ok(BotProcess {
            stdin,
            reader: BufReader::new(stdout),
            alive: true,
        })
    }

    fn send(&mut self, data: &str) {
        if !self.alive {
            return;
        }
        if self.stdin.write_all(data.as_bytes()).is_err() {
            self.alive = false;
            return;
        }
        if self.stdin.flush().is_err() {
            self.alive = false;
        }
    }

    fn read_line(&mut self, _timeout: Duration) -> Option<String> {
        if !self.alive {
            return None;
        }
        // Blocking read. Each match runs in its own rayon thread, so a
        // hung bot only blocks this particular match, not the whole arena.
        // The timeout parameter is reserved for future use.
        let mut line = String::new();
        match self.reader.read_line(&mut line) {
            Ok(n) if n > 0 => Some(line),
            _ => {
                self.alive = false;
                None
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Single match
// ---------------------------------------------------------------------------

fn run_match(
    seed: i64,
    league: i32,
    agent_a: &str,
    agent_b: &str,
    bot_a_is_player_zero: bool,
    max_turns: i32,
    timeout_first: Duration,
    timeout_later: Duration,
    verbose: bool,
) -> MatchResult {
    let mut state = initial_state_from_seed(seed, league);

    let (p0_path, p1_path) = if bot_a_is_player_zero {
        (agent_a, agent_b)
    } else {
        (agent_b, agent_a)
    };

    let mut p0_bot = match BotProcess::spawn(p0_path) {
        Ok(b) => b,
        Err(e) => {
            return MatchResult {
                seed,
                bot_a_is_player_zero,
                body_diff_a: 0,
                winner_a: None,
                turns: 0,
                error: Some(format!("failed to spawn p0 ({}): {}", p0_path, e)),
            };
        }
    };
    let mut p1_bot = match BotProcess::spawn(p1_path) {
        Ok(b) => b,
        Err(e) => {
            return MatchResult {
                seed,
                bot_a_is_player_zero,
                body_diff_a: 0,
                winner_a: None,
                turns: 0,
                error: Some(format!("failed to spawn p1 ({}): {}", p1_path, e)),
            };
        }
    };

    // Send init
    let init_p0 = format_init(&state, 0);
    let init_p1 = format_init(&state, 1);
    p0_bot.send(&init_p0);
    p1_bot.send(&init_p1);

    // Game loop
    while !state.is_terminal(max_turns) {
        let timeout = if state.turn == 0 {
            timeout_first
        } else {
            timeout_later
        };

        let frame = format_frame(&state);
        p0_bot.send(&frame);
        p1_bot.send(&frame);

        let p0_line = p0_bot.read_line(timeout);
        let p1_line = p1_bot.read_line(timeout);

        let p0_action = p0_line
            .as_deref()
            .map(parse_bot_output)
            .unwrap_or_default();
        let p1_action = p1_line
            .as_deref()
            .map(parse_bot_output)
            .unwrap_or_default();

        if verbose {
            eprintln!(
                "[seed={} side={}] turn={} p0={:?} p1={:?}",
                seed,
                if bot_a_is_player_zero { "A=p0" } else { "A=p1" },
                state.turn,
                p0_line.as_deref().map(str::trim),
                p1_line.as_deref().map(str::trim),
            );
        }

        state.step(&p0_action, &p1_action);
    }

    let result = state.final_result(max_turns);
    let body_diff_a = if bot_a_is_player_zero {
        result.body_scores[0] - result.body_scores[1]
    } else {
        result.body_scores[1] - result.body_scores[0]
    };
    let winner_a = result.winner.map(|w| {
        if bot_a_is_player_zero {
            w == 0
        } else {
            w == 1
        }
    });

    MatchResult {
        seed,
        bot_a_is_player_zero,
        body_diff_a,
        winner_a,
        turns: state.turn,
        error: None,
    }
}

// ---------------------------------------------------------------------------
// CLI parsing
// ---------------------------------------------------------------------------

fn parse_seeds(spec: &str) -> Result<Vec<i64>, Box<dyn Error>> {
    // Try as file first
    if let Ok(content) = fs::read_to_string(spec) {
        let mut seeds = Vec::new();
        for line in content.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }
            seeds.push(line.parse()?);
        }
        if !seeds.is_empty() {
            return Ok(seeds);
        }
    }

    // Try as range: "1-60"
    if let Some((left, right)) = spec.split_once('-') {
        if let (Ok(start), Ok(end)) = (left.trim().parse::<i64>(), right.trim().parse::<i64>()) {
            if start <= end {
                return Ok((start..=end).collect());
            }
        }
    }

    // Try as comma-separated list: "1,2,5,10"
    let mut seeds = Vec::new();
    for part in spec.split(',') {
        seeds.push(part.trim().parse()?);
    }
    Ok(seeds)
}

fn parse_args() -> Result<ExtArenaConfig, Box<dyn Error>> {
    let mut agent_a = None;
    let mut agent_b = None;
    let mut seeds_spec = None;
    let mut league = 4_i32;
    let mut jobs = std::thread::available_parallelism()
        .map(|v| v.get().saturating_sub(2).max(1))
        .unwrap_or(1);
    let mut max_turns = 200_i32;
    let mut timeout_first_ms = 5000_u64;
    let mut timeout_later_ms = 1000_u64;
    let mut verbose = false;

    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--agent-a" => {
                agent_a = Some(args.next().ok_or("missing value for --agent-a")?);
            }
            "--agent-b" => {
                agent_b = Some(args.next().ok_or("missing value for --agent-b")?);
            }
            "--seeds" => {
                seeds_spec = Some(args.next().ok_or("missing value for --seeds")?);
            }
            "--league" => {
                league = args.next().ok_or("missing value for --league")?.parse()?;
            }
            "--jobs" | "-j" => {
                jobs = args.next().ok_or("missing value for --jobs")?.parse()?;
            }
            "--max-turns" => {
                max_turns = args
                    .next()
                    .ok_or("missing value for --max-turns")?
                    .parse()?;
            }
            "--timeout-first" => {
                timeout_first_ms = args
                    .next()
                    .ok_or("missing value for --timeout-first")?
                    .parse()?;
            }
            "--timeout-later" => {
                timeout_later_ms = args
                    .next()
                    .ok_or("missing value for --timeout-later")?
                    .parse()?;
            }
            "--verbose" | "-v" => {
                verbose = true;
            }
            _ => return Err(format!("unknown arg: {arg}").into()),
        }
    }

    let agent_a = agent_a.ok_or("missing required --agent-a")?;
    let agent_b = agent_b.ok_or("missing required --agent-b")?;
    let seeds_spec = seeds_spec.ok_or("missing required --seeds")?;
    let seeds = parse_seeds(&seeds_spec)?;
    if seeds.is_empty() {
        return Err("no seeds specified".into());
    }

    Ok(ExtArenaConfig {
        agent_a,
        agent_b,
        seeds,
        league,
        jobs: jobs.max(1),
        max_turns,
        timeout_first_ms,
        timeout_later_ms,
        verbose,
    })
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() -> Result<(), Box<dyn Error>> {
    let config = parse_args()?;

    let n_seeds = config.seeds.len();
    let total_games = n_seeds * 2;

    eprintln!(
        "=== ext_arena: {} vs {} ===",
        config.agent_a, config.agent_b
    );
    eprintln!(
        "Seeds: {} ({}), League: {}, Jobs: {}, MaxTurns: {}",
        n_seeds,
        if n_seeds <= 10 {
            config
                .seeds
                .iter()
                .map(|s| s.to_string())
                .collect::<Vec<_>>()
                .join(",")
        } else {
            format!(
                "{}..{}",
                config.seeds.first().unwrap(),
                config.seeds.last().unwrap()
            )
        },
        config.league,
        config.jobs,
        config.max_turns,
    );

    // Build task list: each seed played from both sides
    let tasks: Vec<(i64, bool)> = config
        .seeds
        .iter()
        .flat_map(|&seed| [(seed, true), (seed, false)])
        .collect();

    let start_time = Instant::now();

    let timeout_first = Duration::from_millis(config.timeout_first_ms);
    let timeout_later = Duration::from_millis(config.timeout_later_ms);

    let pool = rayon::ThreadPoolBuilder::new()
        .num_threads(config.jobs)
        .build()?;

    let results: Vec<MatchResult> = pool.install(|| {
        tasks
            .par_iter()
            .map(|(seed, bot_a_is_p0)| {
                run_match(
                    *seed,
                    config.league,
                    &config.agent_a,
                    &config.agent_b,
                    *bot_a_is_p0,
                    config.max_turns,
                    timeout_first,
                    timeout_later,
                    config.verbose,
                )
            })
            .collect()
    });

    let elapsed = start_time.elapsed();

    // Summarize
    let mut wins = 0_usize;
    let mut losses = 0_usize;
    let mut draws = 0_usize;
    let mut total_body_diff = 0_i64;
    let mut errors = 0_usize;

    for r in &results {
        if r.error.is_some() {
            errors += 1;
            if config.verbose {
                eprintln!("[ERROR] seed={} side={}: {}", r.seed,
                    if r.bot_a_is_player_zero { "A=p0" } else { "A=p1" },
                    r.error.as_deref().unwrap_or("?"));
            }
            continue;
        }
        total_body_diff += i64::from(r.body_diff_a);
        match r.winner_a {
            Some(true) => wins += 1,
            Some(false) => losses += 1,
            None => draws += 1,
        }
    }

    let played = results.len() - errors;
    let avg_body_diff = if played > 0 {
        total_body_diff as f64 / played as f64
    } else {
        0.0
    };

    println!();
    println!(
        "=== ext_arena: {} vs {} ===",
        config.agent_a, config.agent_b
    );
    println!(
        "Seeds: {}, League: {}, Jobs: {}",
        n_seeds, config.league, config.jobs
    );
    println!("Games: {} ({} seeds x 2 sides)", total_games, n_seeds);
    if errors > 0 {
        println!("Errors: {}", errors);
    }
    println!("W-L-D: {}-{}-{}", wins, losses, draws);
    println!("Win rate: {:.1}%", wins as f64 / played.max(1) as f64 * 100.0);
    println!("Avg body diff: {:+.1}", avg_body_diff);
    println!("Time: {:.1}s", elapsed.as_secs_f64());

    Ok(())
}
