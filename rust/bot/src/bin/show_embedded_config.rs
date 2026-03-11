use serde::Serialize;
use snakebot_bot::config::BotConfig;

#[derive(Serialize)]
struct EmbeddedConfigInfo<'a> {
    name: String,
    config_hash: &'a str,
    source_path: &'a str,
}

fn main() {
    let config = BotConfig::embedded();
    let info = EmbeddedConfigInfo {
        name: config.name,
        config_hash: BotConfig::embedded_hash(),
        source_path: BotConfig::embedded_source_path(),
    };
    println!(
        "{}",
        serde_json::to_string_pretty(&info).expect("embedded config info serializable")
    );
}
