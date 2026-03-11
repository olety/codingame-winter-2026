use std::env;
use std::error::Error;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::PathBuf;

use snakebot_bot::config::BotConfig;
use snakebot_bot::features::{encode_training_row, TrainingMetadata};
use snakebot_bot::search::SearchBudget;
use snakebot_bot::selfplay::{play_and_collect, SelfPlayConfig};
use snakebot_engine::load_dump_records;

const DEFAULT_SCHEMA_VERSION: u32 = 2;

struct ExportConfig {
    maps_path: PathBuf,
    out_path: PathBuf,
    limit: usize,
    max_turns: usize,
    budget: SearchBudget,
    shard_index: usize,
    num_shards: usize,
    git_sha: String,
    bot_config: BotConfig,
}

fn main() -> Result<(), Box<dyn Error>> {
    let config = parse_args()?;
    let records = load_dump_records(&config.maps_path)?;
    let config_hash = stable_hash(&serde_json::to_vec(&config.bot_config)?);
    let mut writer = BufWriter::new(File::create(&config.out_path)?);
    let mut emitted = 0_usize;
    let mut games = 0_usize;

    for (index, record) in records.into_iter().enumerate() {
        if index >= config.limit {
            break;
        }
        if index % config.num_shards != config.shard_index {
            continue;
        }
        let game_id = format!("seed-{}-league-{}", record.seed, record.league_level);
        let collected = play_and_collect(
            record.state.to_game_state(),
            &SelfPlayConfig {
                max_turns: config.max_turns,
                budget: config.budget,
                bot_config: config.bot_config.clone(),
            },
        );
        for position in collected.positions {
            let row = encode_training_row(
                &position.state,
                position.owner,
                &collected.final_result,
                TrainingMetadata {
                    schema_version: DEFAULT_SCHEMA_VERSION,
                    git_sha: config.git_sha.clone(),
                    config_hash: config_hash.clone(),
                    seed: record.seed,
                    game_id: game_id.clone(),
                    turn: position.turn,
                    chosen_action_id: position.outcome.action_id,
                    joint_action_count: position.outcome.action_count,
                    root_values: position.outcome.root_values,
                    search_stats: position.outcome.stats,
                },
            );
            serde_json::to_writer(&mut writer, &row)?;
            writer.write_all(b"\n")?;
            emitted += 1;
        }
        games += 1;
    }
    writer.flush()?;

    eprintln!(
        "exported {emitted} samples from {games} game(s) to {}",
        config.out_path.display()
    );
    Ok(())
}

fn parse_args() -> Result<ExportConfig, Box<dyn Error>> {
    let mut maps_path = None;
    let mut out_path = None;
    let mut limit = usize::MAX;
    let mut max_turns = 200;
    let mut budget = SearchBudget::ExtraNodesAfterRoot(5_000);
    let mut shard_index = 0_usize;
    let mut num_shards = 1_usize;
    let mut git_sha = "unknown".to_owned();
    let mut bot_config = BotConfig::default();
    let mut args = env::args().skip(1);

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--maps" => {
                maps_path = Some(PathBuf::from(
                    args.next().ok_or("missing value for --maps")?,
                ))
            }
            "--out" => {
                out_path = Some(PathBuf::from(args.next().ok_or("missing value for --out")?))
            }
            "--limit" => limit = args.next().ok_or("missing value for --limit")?.parse()?,
            "--max-turns" => {
                max_turns = args
                    .next()
                    .ok_or("missing value for --max-turns")?
                    .parse()?
            }
            "--search-ms" => {
                budget = SearchBudget::TimeMs(
                    args.next()
                        .ok_or("missing value for --search-ms")?
                        .parse()?,
                )
            }
            "--extra-nodes-after-root" => {
                budget = SearchBudget::ExtraNodesAfterRoot(
                    args.next()
                        .ok_or("missing value for --extra-nodes-after-root")?
                        .parse()?,
                )
            }
            "--shard-index" => {
                shard_index = args
                    .next()
                    .ok_or("missing value for --shard-index")?
                    .parse()?
            }
            "--num-shards" => {
                num_shards = args
                    .next()
                    .ok_or("missing value for --num-shards")?
                    .parse()?
            }
            "--git-sha" => git_sha = args.next().ok_or("missing value for --git-sha")?,
            "--config" => {
                let path = args.next().ok_or("missing value for --config")?;
                bot_config = BotConfig::load(path)?;
            }
            _ => return Err(format!("unknown arg: {arg}").into()),
        }
    }

    if num_shards == 0 {
        return Err("num shards must be positive".into());
    }
    if shard_index >= num_shards {
        return Err(
            format!("shard index {shard_index} out of range for {num_shards} shards").into(),
        );
    }

    Ok(ExportConfig {
        maps_path: maps_path.ok_or("missing required --maps")?,
        out_path: out_path.ok_or("missing required --out")?,
        limit,
        max_turns,
        budget,
        shard_index,
        num_shards,
        git_sha,
        bot_config,
    })
}

fn stable_hash(bytes: &[u8]) -> String {
    let mut hash = 0xcbf29ce484222325_u64;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    format!("{hash:016x}")
}
