use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-env-changed=SNAKEBOT_CONFIG_PATH");

    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("manifest dir"));
    let default_path = manifest_dir.join("configs/submission_current.json");
    println!("cargo:rerun-if-changed={}", default_path.display());

    let config_path = env::var_os("SNAKEBOT_CONFIG_PATH")
        .map(PathBuf::from)
        .unwrap_or(default_path);
    let config_path = if config_path.is_absolute() {
        config_path
    } else if config_path.exists() {
        config_path
    } else if manifest_dir
        .parent()
        .and_then(|path| path.parent())
        .map(|workspace_root| workspace_root.join(&config_path).exists())
        .unwrap_or(false)
    {
        manifest_dir
            .parent()
            .and_then(|path| path.parent())
            .expect("workspace root")
            .join(config_path)
    } else {
        manifest_dir.join(config_path)
    };
    let config_path = config_path
        .canonicalize()
        .unwrap_or_else(|err| panic!("failed to resolve embedded config path: {err}"));
    println!("cargo:rerun-if-changed={}", config_path.display());

    let raw = fs::read(&config_path).unwrap_or_else(|err| {
        panic!(
            "failed to read embedded config {}: {err}",
            config_path.display()
        )
    });
    println!(
        "cargo:rustc-env=SNAKEBOT_EMBEDDED_CONFIG_PATH={}",
        config_path.display()
    );
    println!(
        "cargo:rustc-env=SNAKEBOT_EMBEDDED_CONFIG_HASH={}",
        hash_bytes(&raw)
    );
}

fn hash_bytes(bytes: &[u8]) -> String {
    let mut hash = 0xcbf29ce484222325_u64;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    format!("{hash:016x}")
}
