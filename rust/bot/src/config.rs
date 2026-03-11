use std::fs;
use std::path::Path;

use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
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

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
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

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BotConfig {
    pub name: String,
    pub eval: EvalWeights,
    pub search: SearchConfig,
}

impl Default for BotConfig {
    fn default() -> Self {
        Self {
            name: "search_v2".to_owned(),
            eval: EvalWeights::default(),
            search: SearchConfig::default(),
        }
    }
}

impl BotConfig {
    pub fn load(path: impl AsRef<Path>) -> Result<Self, Box<dyn std::error::Error>> {
        let raw = fs::read_to_string(path)?;
        Ok(serde_json::from_str(&raw)?)
    }
}
