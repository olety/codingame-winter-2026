use std::fs;
use std::path::Path;

use serde::{Deserialize, Serialize};

const EMBEDDED_CONFIG_JSON: &str = include_str!(env!("SNAKEBOT_EMBEDDED_CONFIG_PATH"));

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct EvalWeights {
    pub body: f64,
    pub loss: f64,
    pub mobility: f64,
    pub apple: f64,
    pub stability: f64,
    pub breakpoint: f64,
    pub fragile_attack: f64,
    pub terminal: f64,
}

impl Default for EvalWeights {
    fn default() -> Self {
        Self {
            body: 120.0,
            loss: 18.0,
            mobility: 7.5,
            apple: 16.0,
            stability: 10.0,
            breakpoint: 9.0,
            fragile_attack: 8.0,
            terminal: 10_000.0,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct SearchConfig {
    pub first_turn_ms: u64,
    pub later_turn_ms: u64,
    pub deepen_top_my: usize,
    pub deepen_top_opp: usize,
    pub deepen_child_my: usize,
    pub deepen_child_opp: usize,
    pub extra_nodes_after_root: u64,
}

impl Default for SearchConfig {
    fn default() -> Self {
        Self {
            first_turn_ms: 850,
            later_turn_ms: 40,
            deepen_top_my: 6,
            deepen_top_opp: 6,
            deepen_child_my: 4,
            deepen_child_opp: 4,
            extra_nodes_after_root: 5_000,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct BotConfig {
    pub name: String,
    pub eval: EvalWeights,
    pub search: SearchConfig,
}

impl Default for BotConfig {
    fn default() -> Self {
        Self::embedded()
    }
}

impl BotConfig {
    pub fn embedded() -> Self {
        serde_json::from_str(EMBEDDED_CONFIG_JSON).expect("embedded bot config should parse")
    }

    pub fn embedded_hash() -> &'static str {
        env!("SNAKEBOT_EMBEDDED_CONFIG_HASH")
    }

    pub fn embedded_source_path() -> &'static str {
        env!("SNAKEBOT_EMBEDDED_CONFIG_PATH")
    }

    pub fn load(path: impl AsRef<Path>) -> Result<Self, Box<dyn std::error::Error>> {
        let raw = fs::read_to_string(path)?;
        Ok(serde_json::from_str(&raw)?)
    }
}

pub fn hash_config_bytes(bytes: &[u8]) -> String {
    let mut hash = 0xcbf29ce484222325_u64;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    format!("{hash:016x}")
}

pub fn hash_config_file(path: impl AsRef<Path>) -> Result<String, Box<dyn std::error::Error>> {
    Ok(hash_config_bytes(&fs::read(path)?))
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::{hash_config_bytes, BotConfig};

    #[test]
    fn embedded_config_matches_submission_file() {
        let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        let path = manifest_dir.join("configs/submission_current.json");
        let raw = std::fs::read_to_string(&path).expect("submission config should exist");
        let expected: BotConfig = serde_json::from_str(&raw).expect("submission config parses");
        assert_eq!(BotConfig::embedded(), expected);
        assert_eq!(
            BotConfig::embedded_hash(),
            hash_config_bytes(raw.as_bytes())
        );
    }
}
