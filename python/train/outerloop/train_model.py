from __future__ import annotations

import argparse
import json
import math
import os
import random
import time
from pathlib import Path

import torch
from torch import nn
from torch.utils.data import DataLoader, Subset

from python.train.outerloop.dataset import BitpackedDataset, BitpackedDistillDataset, HybridDistillDataset, HybridSelfPlayDataset, dedup_dataset, dedup_rows, grouped_split_indices, grouped_split_indices_bitpacked
from python.train.outerloop.model import TeacherHybridNet, TinyHybridNet


def _make_optimizer(model: nn.Module, spec: dict) -> torch.optim.Optimizer:
    name = spec.get("optimizer", "adamw")
    if name == "muon":
        lr = float(spec.get("learning_rate", 0.02))
        momentum = float(spec.get("muon_momentum", 0.95))
        wd = float(spec.get("weight_decay", 1e-4))
        # Native torch.optim.Muon (2.9+) only supports 2D params — use for MLPs only
        has_conv = any(p.ndim > 2 for p in model.parameters())
        if hasattr(torch.optim, "Muon") and not has_conv:
            return torch.optim.Muon(
                model.parameters(), lr=lr, momentum=momentum,
                weight_decay=wd, nesterov=True,
            )
        # Custom Muon handles conv weights via internal reshaping
        from muon import SingleDeviceMuonWithAuxAdam
        muon_params = [p for p in model.parameters() if p.ndim >= 2]
        other_params = [p for p in model.parameters() if p.ndim < 2]
        param_groups = [
            dict(params=muon_params, lr=lr, momentum=momentum,
                 weight_decay=wd, use_muon=True),
            dict(params=other_params, lr=float(spec.get("adamw_lr", 3e-4)),
                 betas=(0.9, 0.95), weight_decay=wd, use_muon=False),
        ]
        return SingleDeviceMuonWithAuxAdam(param_groups)
    return torch.optim.AdamW(
        model.parameters(),
        lr=float(spec.get("learning_rate", 1e-3)),
        weight_decay=float(spec.get("weight_decay", 1e-4)),
    )


def select_device(preference: str) -> torch.device:
    if preference == "cuda" and torch.cuda.is_available():
        return torch.device("cuda")
    if preference == "mps" and torch.backends.mps.is_available():
        return torch.device("mps")
    return torch.device("cpu")


def set_seed(seed: int) -> None:
    random.seed(seed)
    torch.manual_seed(seed)


def accuracy(policy_logits: torch.Tensor, policy_targets: torch.Tensor) -> tuple[int, int]:
    predictions = policy_logits.argmax(dim=-1)
    mask = policy_targets >= 0
    if mask.sum().item() == 0:
        return 0, 0
    correct = (predictions[mask] == policy_targets[mask]).sum().item()
    total = mask.sum().item()
    return int(correct), int(total)


def pearson(preds: list[float], targets: list[float]) -> float:
    if len(preds) < 2:
        return 0.0
    pred_mean = sum(preds) / len(preds)
    targ_mean = sum(targets) / len(targets)
    num = sum((p - pred_mean) * (t - targ_mean) for p, t in zip(preds, targets))
    pred_den = math.sqrt(sum((p - pred_mean) ** 2 for p in preds))
    targ_den = math.sqrt(sum((t - targ_mean) ** 2 for t in targets))
    denom = pred_den * targ_den
    return 0.0 if denom == 0 else num / denom


def evaluate(model: nn.Module, loader: DataLoader, device: torch.device) -> dict:
    model.eval()
    mae_total = 0.0
    preds: list[float] = []
    targets: list[float] = []
    policy_correct = 0
    policy_total = 0
    with torch.no_grad():
        for batch in loader:
            grid = batch["grid"].to(device)
            scalars = batch["scalars"].to(device)
            target = batch["value"].to(device)
            policy_targets = batch["policy_targets"].to(device)
            policy_logits, pred = model(grid, scalars)
            mae_total += torch.abs(pred - target).mean().item()
            preds.extend(pred.squeeze(1).cpu().tolist())
            targets.extend(target.squeeze(1).cpu().tolist())
            correct, total = accuracy(policy_logits, policy_targets)
            policy_correct += correct
            policy_total += total
    return {
        "value_mae": mae_total / max(len(loader), 1),
        "value_correlation": pearson(preds, targets),
        "policy_accuracy": policy_correct / max(policy_total, 1),
    }


def train_from_spec(spec: dict) -> dict:
    seed = int(spec.get("seed", 42))
    set_seed(seed)
    device = select_device(str(spec.get("device_preference", "mps")))
    started = time.time()

    dataset = HybridSelfPlayDataset(spec["dataset_path"], max_samples=int(spec.get("max_samples", 0)))
    dataset.rows = dedup_rows(dataset.rows)
    train_indices, valid_indices = grouped_split_indices(
        dataset.rows,
        float(spec.get("train_split", 0.9)),
        seed,
    )

    train_set = Subset(dataset, train_indices)
    valid_set = Subset(dataset, valid_indices)
    batch_size = int(spec.get("batch_size", 128))
    train_loader = DataLoader(train_set, batch_size=batch_size, shuffle=True)
    valid_loader = DataLoader(valid_set, batch_size=batch_size, shuffle=False)

    sample = dataset[0]
    grid_shape = sample["grid"].shape
    scalars_shape = sample["scalars"].shape
    model = TinyHybridNet(
        input_channels=int(grid_shape[0]),
        scalar_features=int(scalars_shape[0]),
        conv_channels=int(spec.get("conv_channels", 8)),
        num_conv_layers=int(spec.get("num_conv_layers", 2)),
    ).to(device)
    optimizer = _make_optimizer(model, spec)
    value_loss_fn = nn.SmoothL1Loss(reduction="none")
    policy_loss_fn = nn.CrossEntropyLoss(ignore_index=-100)

    train_value_loss = 0.0
    train_policy_loss = 0.0
    for _ in range(int(spec.get("epochs", 4))):
        model.train()
        for batch in train_loader:
            grid = batch["grid"].to(device)
            scalars = batch["scalars"].to(device)
            target = batch["value"].to(device)
            weight = batch["weight"].to(device)
            policy_targets = batch["policy_targets"].to(device)
            policy_logits, pred = model(grid, scalars)
            value_loss = (value_loss_fn(pred, target) * weight).mean()
            policy_loss = policy_loss_fn(
                policy_logits.view(-1, policy_logits.shape[-1]),
                policy_targets.view(-1),
            )
            loss = value_loss + float(spec.get("policy_loss_weight", 1.0)) * policy_loss
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            train_value_loss = value_loss.item()
            train_policy_loss = policy_loss.item()

    validation = evaluate(model, valid_loader, device)
    output_dir = Path(spec["output_dir"])
    output_dir.mkdir(parents=True, exist_ok=True)
    model_path = output_dir / "hybrid_model.pt"
    torch.save(model.state_dict(), model_path)
    training_config = {
        "input_channels": int(grid_shape[0]),
        "scalar_features": int(scalars_shape[0]),
        "board_height": int(grid_shape[1]),
        "board_width": int(grid_shape[2]),
        "conv_channels": int(spec.get("conv_channels", 8)),
        "num_conv_layers": int(spec.get("num_conv_layers", 2)),
    }
    training_config_path = output_dir / "training_config.json"
    training_config_path.write_text(json.dumps(training_config, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    metrics = {
        "device": str(device),
        "samples": len(dataset.rows),
        "train_samples": len(train_indices),
        "valid_samples": len(valid_indices),
        "train_value_loss": train_value_loss,
        "train_policy_loss": train_policy_loss,
        "validation_value_mae": validation["value_mae"],
        "validation_value_correlation": validation["value_correlation"],
        "validation_policy_accuracy": validation["policy_accuracy"],
        "training_wallclock_minutes": (time.time() - started) / 60.0,
        "model_path": str(model_path),
        "training_config_path": str(training_config_path),
    }
    metrics_path = output_dir / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return metrics


def train_teacher_from_spec(spec: dict) -> dict:
    """Train a larger teacher model on the dataset."""
    import torch.nn.functional as F

    seed = int(spec.get("seed", 42))
    set_seed(seed)
    device = select_device(str(spec.get("device_preference", "cuda")))
    started = time.time()

    print(f"[teacher] Loading dataset from {spec['dataset_path']}...", flush=True)
    load_start = time.time()
    dataset_path = Path(spec["dataset_path"])
    use_bitpacked = dataset_path.is_dir() and (dataset_path / "header.json").exists()
    if use_bitpacked:
        dataset = BitpackedDataset(dataset_path)
        print(f"[teacher] Bitpacked dataset: {len(dataset):,} rows in {time.time()-load_start:.0f}s (memory-mapped)", flush=True)
        train_indices, valid_indices = grouped_split_indices_bitpacked(
            dataset._seeds, dataset._game_id_hashes,
            float(spec.get("train_split", 0.9)), seed,
        )
    else:
        dataset = HybridSelfPlayDataset(spec["dataset_path"], max_samples=int(spec.get("max_samples", 0)))
        print(f"[teacher] Loaded {len(dataset.rows)} rows in {time.time()-load_start:.0f}s, deduplicating...", flush=True)
        dedup_dataset(dataset)
        print(f"[teacher] {len(dataset.rows)} unique rows", flush=True)
        train_indices, valid_indices = grouped_split_indices(
            dataset.rows, float(spec.get("train_split", 0.9)), seed,
        )
    batch_size = int(spec.get("batch_size", 256))
    use_cuda = device.type == "cuda"
    num_workers = int(spec.get("num_workers", 8 if use_bitpacked else 0))
    loader_kwargs = dict(pin_memory=use_cuda, num_workers=num_workers)
    if num_workers > 0:
        loader_kwargs.update(persistent_workers=True, prefetch_factor=3)
    train_loader = DataLoader(Subset(dataset, train_indices), batch_size=batch_size, shuffle=True, drop_last=True, **loader_kwargs)
    valid_loader = DataLoader(Subset(dataset, valid_indices), batch_size=batch_size, shuffle=False, **loader_kwargs)

    sample = dataset[0]
    grid_shape = sample["grid"].shape
    scalars_shape = sample["scalars"].shape
    channels = int(spec.get("teacher_conv_channels", 128))
    blocks = int(spec.get("teacher_num_res_blocks", 8))

    # CUDA performance flags
    if device.type == "cuda":
        torch.backends.cudnn.benchmark = True
        torch.set_float32_matmul_precision("high")

    model = TeacherHybridNet(
        input_channels=int(grid_shape[0]),
        scalar_features=int(scalars_shape[0]),
        conv_channels=channels,
        num_res_blocks=blocks,
    ).to(device)

    # NOTE: channels_last disabled — conflicts with Muon optimizer's .view() calls
    # on conv weights. Re-enable after forking Muon to use .reshape().
    # if device.type == "cuda":
    #     model = model.to(memory_format=torch.channels_last)

    param_count = sum(p.numel() for p in model.parameters())
    print(f"[teacher] Model: {channels}ch/{blocks}blocks, {param_count/1e6:.1f}M params, grid={grid_shape}", flush=True)

    # Resume from checkpoint if specified
    resume_ckpt = spec.get("resume_checkpoint")
    start_epoch = 0
    continue_training = bool(spec.get("continue_training", False))
    if resume_ckpt:
        ckpt_path = Path(resume_ckpt)
        print(f"[teacher] Resuming from checkpoint: {ckpt_path}", flush=True)
        ckpt_data = torch.load(ckpt_path, map_location=device, weights_only=False)
        # Support both old (bare state_dict) and new (full checkpoint) formats
        if isinstance(ckpt_data, dict) and "model_state_dict" in ckpt_data:
            model.load_state_dict(ckpt_data["model_state_dict"])
            print(f"[teacher] Checkpoint loaded (epoch {ckpt_data.get('epoch', '?')})", flush=True)
        else:
            model.load_state_dict(ckpt_data)
            ckpt_data = None  # old format, no optimizer/scheduler state
            print(f"[teacher] Checkpoint loaded (legacy format)", flush=True)

    epochs = int(spec.get("epochs", 20))
    optimizer = _make_optimizer(model, spec)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)

    # Continued pre-training: load optimizer state, reset LR schedule (warm restart)
    if resume_ckpt and continue_training and ckpt_data is not None:
        if "optimizer_state_dict" in ckpt_data:
            optimizer.load_state_dict(ckpt_data["optimizer_state_dict"])
            print(f"[teacher] Optimizer state restored for continued training", flush=True)
        start_epoch = ckpt_data.get("epoch", 0)
        # Reset scheduler for warm restart (fresh cosine over new epochs)
        scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)
        print(f"[teacher] Warm restart: {epochs} new epochs from epoch {start_epoch}", flush=True)
    value_loss_fn = nn.SmoothL1Loss(reduction="none")
    policy_loss_fn = nn.CrossEntropyLoss(ignore_index=-100)

    # bf16 autocast — no GradScaler needed for bfloat16
    use_amp = bool(spec.get("use_amp", True)) and device.type == "cuda"
    amp_dtype = torch.bfloat16

    # torch.compile for kernel fusion + Triton optimization
    use_compile = bool(spec.get("use_compile", True)) and device.type == "cuda"
    compile_mode = spec.get("compile_mode", "reduce-overhead")
    if use_compile:
        try:
            # backward_pass_autocast="off" because backward runs outside autocast
            with torch._functorch.config.patch(backward_pass_autocast="off"):
                model = torch.compile(model, mode=compile_mode, dynamic=False)
            print(f"[teacher] torch.compile enabled ({compile_mode}, dynamic=False)", flush=True)
        except Exception as e:
            print(f"[teacher] torch.compile failed, continuing without: {e}", flush=True)
    checkpoint_interval = int(spec.get("checkpoint_interval", 1))
    print(
        f"[teacher] Starting training: {epochs} epochs, {len(train_loader)} batches/epoch, "
        f"device={device}, amp={'bf16' if use_amp else 'off'}"
        + (f", continued from epoch {start_epoch}" if start_epoch > 0 else ""),
        flush=True,
    )
    early_stop_patience = int(spec.get("early_stop_patience", 0))
    best_val_corr = -1.0
    patience_counter = 0
    total_epochs = start_epoch + epochs
    for epoch in range(start_epoch, total_epochs):
        model.train()
        epoch_start = time.time()
        epoch_vloss = torch.tensor(0.0, device=device)
        epoch_ploss = torch.tensor(0.0, device=device)
        n_batches = 0
        for batch in train_loader:
            grid = batch["grid"].to(device, non_blocking=True)
            scalars = batch["scalars"].to(device, non_blocking=True)
            target = batch["value"].to(device, non_blocking=True)
            weight = batch["weight"].to(device, non_blocking=True)
            policy_targets = batch["policy_targets"].to(device, non_blocking=True)
            with torch.autocast("cuda", dtype=amp_dtype, enabled=use_amp):
                policy_logits, pred = model(grid, scalars)
                value_loss = (value_loss_fn(pred, target) * weight).mean()
                policy_loss = policy_loss_fn(
                    policy_logits.view(-1, policy_logits.shape[-1]),
                    policy_targets.view(-1),
                )
                loss = value_loss + policy_loss
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            # Accumulate on GPU to avoid per-batch sync
            epoch_vloss += value_loss.detach()
            epoch_ploss += policy_loss.detach()
            n_batches += 1
            if n_batches % 1000 == 0:
                elapsed = time.time() - epoch_start
                batches_left = len(train_loader) - n_batches
                eta = elapsed / n_batches * batches_left
                avg_vloss = (epoch_vloss / n_batches).item()
                avg_ploss = (epoch_ploss / n_batches).item()
                print(
                    f"[teacher] Epoch {epoch+1} batch {n_batches}/{len(train_loader)} "
                    f"({elapsed:.0f}s, ETA {eta:.0f}s) "
                    f"vloss={avg_vloss:.4f} ploss={avg_ploss:.4f}",
                    flush=True,
                )
        scheduler.step()
        epoch_elapsed = time.time() - epoch_start
        total_elapsed = time.time() - started
        print(
            f"[teacher] Epoch {epoch+1}/{total_epochs} done in {epoch_elapsed:.0f}s "
            f"(total {total_elapsed/60:.1f}min) — "
            f"vloss={(epoch_vloss/max(n_batches,1)).item():.4f} ploss={(epoch_ploss/max(n_batches,1)).item():.4f} "
            f"lr={scheduler.get_last_lr()[0]:.6f}",
            flush=True,
        )
        # Per-epoch validation + checkpoint
        if checkpoint_interval > 0 and (epoch + 1) % checkpoint_interval == 0:
            run_id = spec.get("run_id", "default")
            # Use /data/ (Modal Volume) if available, else output_dir
            base = Path("/data") if Path("/data").exists() and os.access("/data", os.W_OK) else Path(spec["output_dir"])
            ckpt_dir = base / run_id / "teacher" / "checkpoints"
            ckpt_dir.mkdir(parents=True, exist_ok=True)
            ckpt_path = ckpt_dir / f"teacher_epoch{epoch+1}.pt"
            torch.save({
                "model_state_dict": model.state_dict(),
                "optimizer_state_dict": optimizer.state_dict(),
                "epoch": epoch + 1,
            }, ckpt_path)
            # Quick validation every checkpoint
            val_metrics = evaluate(model, valid_loader, device)
            epoch_log = {
                "epoch": epoch + 1,
                "train_vloss": (epoch_vloss / max(n_batches, 1)).item(),
                "train_ploss": (epoch_ploss / max(n_batches, 1)).item(),
                "val_mae": val_metrics["value_mae"],
                "val_corr": val_metrics["value_correlation"],
                "val_policy_acc": val_metrics["policy_accuracy"],
                "epoch_seconds": epoch_elapsed,
                "total_minutes": total_elapsed / 60.0,
                "lr": scheduler.get_last_lr()[0],
            }
            log_path = ckpt_dir.parent / "epoch_logs.jsonl"
            with log_path.open("a", encoding="utf-8") as f:
                f.write(json.dumps(epoch_log) + "\n")
            try:
                from python.train.outerloop.modal_job import vol
                vol.commit()
                print(
                    f"[teacher] Checkpoint epoch {epoch+1}: "
                    f"val_mae={val_metrics['value_mae']:.4f} "
                    f"val_corr={val_metrics['value_correlation']:.4f} "
                    f"val_pacc={val_metrics['policy_accuracy']:.4f}",
                    flush=True,
                )
            except Exception:
                print(f"[teacher] Checkpoint saved locally: {ckpt_path}", flush=True)
            # Early stopping on val_corr
            if early_stop_patience > 0:
                if val_metrics["value_correlation"] > best_val_corr:
                    best_val_corr = val_metrics["value_correlation"]
                    patience_counter = 0
                else:
                    patience_counter += 1
                    print(
                        f"[teacher] Early stop: no improvement for {patience_counter}/{early_stop_patience} epochs "
                        f"(best_val_corr={best_val_corr:.4f})",
                        flush=True,
                    )
                    if patience_counter >= early_stop_patience:
                        print("[teacher] Early stopping triggered — ending training", flush=True)
                        break

    validation = evaluate(model, valid_loader, device)
    output_dir = Path(spec["output_dir"])
    output_dir.mkdir(parents=True, exist_ok=True)
    model_path = output_dir / "teacher_model.pt"
    torch.save(model.state_dict(), model_path)
    training_config = {
        "input_channels": int(grid_shape[0]),
        "scalar_features": int(scalars_shape[0]),
        "board_height": int(grid_shape[1]),
        "board_width": int(grid_shape[2]),
        "teacher_conv_channels": int(spec.get("teacher_conv_channels", 128)),
        "teacher_num_res_blocks": int(spec.get("teacher_num_res_blocks", 8)),
    }
    training_config_path = output_dir / "teacher_training_config.json"
    training_config_path.write_text(json.dumps(training_config, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    metrics = {
        "mode": "teacher",
        "device": str(device),
        "samples": len(dataset),
        "train_samples": len(train_indices),
        "valid_samples": len(valid_indices),
        "epochs": epochs,
        "train_value_loss": (epoch_vloss / max(n_batches, 1)).item(),
        "train_policy_loss": (epoch_ploss / max(n_batches, 1)).item(),
        "validation_value_mae": validation["value_mae"],
        "validation_value_correlation": validation["value_correlation"],
        "validation_policy_accuracy": validation["policy_accuracy"],
        "training_wallclock_minutes": (time.time() - started) / 60.0,
        "model_path": str(model_path),
        "training_config_path": str(training_config_path),
    }
    metrics_path = output_dir / "teacher_metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return metrics


def generate_soft_targets(spec: dict) -> dict:
    """Load teacher model, run inference over dataset, write augmented JSONL."""
    seed = int(spec.get("seed", 42))
    set_seed(seed)
    device = select_device(str(spec.get("device_preference", "cuda")))

    teacher_config = json.loads(Path(spec["teacher_training_config_path"]).read_text(encoding="utf-8"))
    teacher = TeacherHybridNet(
        input_channels=int(teacher_config["input_channels"]),
        scalar_features=int(teacher_config["scalar_features"]),
        conv_channels=int(teacher_config.get("teacher_conv_channels", 128)),
        num_res_blocks=int(teacher_config.get("teacher_num_res_blocks", 8)),
    ).to(device)
    ckpt_data = torch.load(spec["teacher_model_path"], map_location=device, weights_only=False)
    if isinstance(ckpt_data, dict) and "model_state_dict" in ckpt_data:
        sd = ckpt_data["model_state_dict"]
        # Strip _orig_mod. prefix from torch.compile'd checkpoints
        if any(k.startswith("_orig_mod.") for k in sd):
            sd = {k.removeprefix("_orig_mod."): v for k, v in sd.items()}
        teacher.load_state_dict(sd)
        print(f"[soft-targets] Teacher loaded (epoch {ckpt_data.get('epoch', '?')})", flush=True)
    else:
        if isinstance(ckpt_data, dict) and any(k.startswith("_orig_mod.") for k in ckpt_data):
            ckpt_data = {k.removeprefix("_orig_mod."): v for k, v in ckpt_data.items()}
        teacher.load_state_dict(ckpt_data)
        print("[soft-targets] Teacher loaded (legacy format)", flush=True)
    teacher.eval()

    import numpy as np

    max_samples = int(spec.get("max_samples", 0))
    dataset_path = Path(spec["dataset_path"])
    use_bitpacked = dataset_path.is_dir() and (dataset_path / "header.json").exists()
    if use_bitpacked:
        dataset = BitpackedDataset(dataset_path)
        print(f"[soft-targets] Bitpacked dataset: {len(dataset):,} rows (memory-mapped)", flush=True)
    else:
        dataset = HybridSelfPlayDataset(spec["dataset_path"], max_samples=max_samples)
        print(f"[soft-targets] JSONL dataset: {len(dataset)} rows", flush=True)

    # Subsample and preload into memory (avoids slow random mmap on network mounts)
    sampled_indices = None
    if max_samples and max_samples < len(dataset):
        import random as _rng
        _rng.seed(seed)
        sampled_indices = sorted(_rng.sample(range(len(dataset)), max_samples))
        print(f"[soft-targets] Preloading {max_samples:,} rows into memory...", flush=True)
        preload_start = time.time()
        grids = torch.zeros(max_samples, 19, 24, 44, dtype=torch.float32)
        scalars_all = torch.zeros(max_samples, 6, dtype=torch.float32)
        values_all = torch.zeros(max_samples, 1, dtype=torch.float32)
        policy_all = torch.zeros(max_samples, 4, dtype=torch.long)
        weights_all = torch.zeros(max_samples, 1, dtype=torch.float32)
        for out_idx, src_idx in enumerate(sampled_indices):
            item = dataset[src_idx]
            grids[out_idx] = item["grid"]
            scalars_all[out_idx] = item["scalars"]
            values_all[out_idx] = item["value"]
            policy_all[out_idx] = item["policy_targets"]
            weights_all[out_idx] = item["weight"]
            if (out_idx + 1) % 200_000 == 0:
                print(f"[soft-targets] Preloaded {out_idx+1:,}/{max_samples:,}...", flush=True)
        preloaded = torch.utils.data.TensorDataset(grids, scalars_all, values_all, policy_all, weights_all)
        print(f"[soft-targets] Preloaded {max_samples:,} rows in {time.time()-preload_start:.0f}s", flush=True)
        loader = DataLoader(preloaded, batch_size=int(spec.get("batch_size", 512)), shuffle=False)
    else:
        use_cuda = device.type == "cuda"
        loader_kwargs: dict = dict(pin_memory=use_cuda)
        num_workers = int(spec.get("num_workers", 4 if use_cuda else 0))
        if num_workers > 0:
            loader_kwargs.update(num_workers=num_workers, persistent_workers=True, prefetch_factor=2)
        loader = DataLoader(dataset, batch_size=int(spec.get("batch_size", 512)), shuffle=False, **loader_kwargs)

    # Output as compact numpy arrays: indices, teacher_policy_logits, teacher_values
    # This avoids the 160GB JSON problem (only ~170MB for 2M rows)
    output_path = Path(spec["output_path"])
    output_dir = output_path.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    n_total = len(loader.dataset)
    all_policy = np.zeros((n_total, 4, 5), dtype=np.float16)
    all_values = np.zeros((n_total,), dtype=np.float16)
    row_idx = 0
    with torch.no_grad():
        for batch in loader:
            # Handle both dict (BitpackedDataset) and tuple (TensorDataset) formats
            if isinstance(batch, (list, tuple)):
                grid, scalars_b = batch[0].to(device), batch[1].to(device)
            else:
                grid, scalars_b = batch["grid"].to(device), batch["scalars"].to(device)
            policy_logits, value_pred = teacher(grid, scalars_b)
            bs = grid.shape[0]
            all_policy[row_idx:row_idx + bs] = policy_logits.cpu().numpy().astype(np.float16)
            all_values[row_idx:row_idx + bs] = value_pred[:, 0].cpu().numpy().astype(np.float16)
            row_idx += bs
            if row_idx % 100_000 < bs:
                print(f"[soft-targets] Processed {row_idx:,}/{n_total:,} rows...", flush=True)

    # Save as .npz with metadata
    indices_arr = np.array(sampled_indices, dtype=np.int32) if sampled_indices else np.arange(n_total, dtype=np.int32)
    npz_path = output_dir / "teacher_predictions.npz"
    np.savez_compressed(npz_path,
                        indices=indices_arr,
                        teacher_policy_logits=all_policy[:row_idx],
                        teacher_values=all_values[:row_idx])
    # Also save metadata
    meta = {
        "dataset_path": spec["dataset_path"],
        "teacher_model_path": spec["teacher_model_path"],
        "num_samples": row_idx,
        "seed": seed,
    }
    (output_dir / "teacher_predictions_meta.json").write_text(
        json.dumps(meta, indent=2) + "\n", encoding="utf-8"
    )

    file_size_mb = npz_path.stat().st_size / 1024 / 1024
    print(f"[soft-targets] Done: {row_idx:,} rows → {npz_path} ({file_size_mb:.1f} MB)", flush=True)
    return {
        "mode": "soft-targets",
        "output_path": str(npz_path),
        "rows_processed": row_idx,
    }


def train_distill_from_spec(spec: dict) -> dict:
    """Train student via distillation from teacher soft targets."""
    import torch.nn.functional as F

    seed = int(spec.get("seed", 42))
    set_seed(seed)
    device = select_device(str(spec.get("device_preference", "mps")))
    started = time.time()

    dataset_path = Path(spec["dataset_path"])
    use_bitpacked_distill = dataset_path.is_dir() and (dataset_path / "teacher_predictions.npz").exists()
    if use_bitpacked_distill:
        preload = bool(spec.get("preload_dataset", True))
        base_override = spec.get("bitpacked_base_path")
        max_samples = int(spec.get("max_samples", 0))
        chunk_offset = int(spec.get("chunk_offset", 0))
        dataset = BitpackedDistillDataset(
            dataset_path, base_path_override=base_override,
            preload=preload, max_samples=max_samples, chunk_offset=chunk_offset,
        )
        train_indices, valid_indices = grouped_split_indices_bitpacked(
            dataset._seeds, dataset._game_id_hashes,
            float(spec.get("train_split", 0.9)), seed,
        )
    else:
        dataset = HybridDistillDataset(spec["dataset_path"], max_samples=int(spec.get("max_samples", 0)))
        dataset.rows = dedup_rows(dataset.rows)
        train_indices, valid_indices = grouped_split_indices(
            dataset.rows, float(spec.get("train_split", 0.9)), seed,
        )
    batch_size = int(spec.get("batch_size", 128))
    num_workers = int(spec.get("num_workers", 0))
    pin = device.type == "cuda"  # pin_memory only for CUDA, not MPS
    train_loader = DataLoader(
        Subset(dataset, train_indices), batch_size=batch_size, shuffle=True,
        num_workers=num_workers, pin_memory=pin,
    )
    valid_loader = DataLoader(
        Subset(dataset, valid_indices), batch_size=batch_size, shuffle=False,
        num_workers=num_workers, pin_memory=pin,
    )

    sample = dataset[0]
    grid_shape = sample["grid"].shape
    scalars_shape = sample["scalars"].shape
    use_qat = bool(spec.get("qat_bits"))
    model = TinyHybridNet(
        input_channels=int(grid_shape[0]),
        scalar_features=int(scalars_shape[0]),
        conv_channels=int(spec.get("conv_channels", 8)),
        num_conv_layers=int(spec.get("num_conv_layers", 2)),
        use_qat=use_qat,
    ).to(device)
    if use_qat:
        print(f"[distill] Int{spec['qat_bits']} QAT enabled", flush=True)

    T = float(spec.get("distill_temperature", 3.0))
    alpha = float(spec.get("distill_alpha", 0.5))
    epochs = int(spec.get("epochs", 4))
    optimizer = _make_optimizer(model, spec)
    hard_policy_loss_fn = nn.CrossEntropyLoss(ignore_index=-100)

    # Resume from checkpoint if provided
    start_epoch = 0
    resume_path = spec.get("resume_checkpoint")
    if resume_path and Path(resume_path).exists():
        ckpt = torch.load(resume_path, map_location=device, weights_only=False)
        state = ckpt.get("model_state_dict", ckpt)
        # Strip _orig_mod. prefix from torch.compile checkpoints
        state = {k.replace("_orig_mod.", ""): v for k, v in state.items()}
        model.load_state_dict(state)
        if "optimizer_state_dict" in ckpt:
            optimizer.load_state_dict(ckpt["optimizer_state_dict"])
        start_epoch = ckpt.get("epoch", 0)
        print(f"[distill] Resumed from {resume_path} at epoch {start_epoch}", flush=True)

    early_stop_patience = int(spec.get("early_stop_patience", 0))
    best_val_pacc = 0.0
    epochs_without_improvement = 0

    print(
        f"[distill] Training: epochs {start_epoch+1}-{epochs}, {len(train_loader)} batches/epoch, "
        f"T={T}, alpha={alpha}, device={device}" + (f", qat=int{spec['qat_bits']}" if use_qat else "")
        + (f", early_stop_patience={early_stop_patience}" if early_stop_patience else ""),
        flush=True,
    )
    train_total_loss = 0.0
    for epoch in range(start_epoch, epochs):
        model.train()
        epoch_start = time.time()
        epoch_loss = 0.0
        n_batches = 0
        for batch in train_loader:
            grid = batch["grid"].to(device)
            scalars = batch["scalars"].to(device)
            target_value = batch["value"].to(device)
            policy_targets = batch["policy_targets"].to(device)
            student_policy, student_value = model(grid, scalars)

            loss = torch.tensor(0.0, device=device)

            if "teacher_policy_logits" in batch:
                teacher_policy = batch["teacher_policy_logits"].to(device)
                teacher_value = batch["teacher_value"].to(device)
                student_flat = student_policy.view(-1, student_policy.shape[-1])
                teacher_flat = teacher_policy.view(-1, teacher_policy.shape[-1])
                kl_loss = F.kl_div(
                    F.log_softmax(student_flat / T, dim=-1),
                    F.softmax(teacher_flat / T, dim=-1),
                    reduction="batchmean",
                )
                loss = loss + (T * T) * kl_loss
                loss = loss + 1.5 * F.mse_loss(student_value, teacher_value)

            hard_loss = hard_policy_loss_fn(
                student_policy.view(-1, student_policy.shape[-1]),
                policy_targets.view(-1),
            )
            loss = loss + alpha * hard_loss

            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            epoch_loss += loss.item()
            n_batches += 1
            if n_batches % 5000 == 0:
                elapsed = time.time() - epoch_start
                eta = elapsed / n_batches * (len(train_loader) - n_batches)
                print(
                    f"[distill] Epoch {epoch+1}/{epochs} batch {n_batches}/{len(train_loader)} "
                    f"loss={epoch_loss/n_batches:.4f} ({elapsed:.0f}s, ETA {eta:.0f}s)",
                    flush=True,
                )
        train_total_loss = epoch_loss / max(n_batches, 1)
        epoch_elapsed = time.time() - epoch_start
        total_elapsed = time.time() - started

        # Per-epoch validation
        val_metrics = evaluate(model, valid_loader, device)
        print(
            f"[distill] Epoch {epoch+1}/{epochs} done in {epoch_elapsed:.0f}s "
            f"(total {total_elapsed/60:.1f}min) — "
            f"loss={train_total_loss:.4f} "
            f"val_corr={val_metrics['value_correlation']:.4f} "
            f"val_pacc={val_metrics['policy_accuracy']:.4f}",
            flush=True,
        )

        # Checkpoint — to output_dir locally, or /data on Modal
        ckpt_base = spec.get("checkpoint_dir")
        if not ckpt_base:
            run_id = spec.get("run_id", "default")
            vol_base = Path("/data") if Path("/data").exists() and os.access("/data", os.W_OK) else None
            if vol_base is not None:
                ckpt_base = str(vol_base / run_id / "checkpoints")
        if not ckpt_base:
            ckpt_base = str(Path(spec["output_dir"]) / "checkpoints")
        ckpt_dir = Path(ckpt_base)
        ckpt_dir.mkdir(parents=True, exist_ok=True)
        ckpt_path = ckpt_dir / f"distill_epoch{epoch+1}.pt"
        torch.save({
            "model_state_dict": model.state_dict(),
            "optimizer_state_dict": optimizer.state_dict(),
            "epoch": epoch + 1,
        }, ckpt_path)
        print(f"[distill] Checkpoint saved: {ckpt_path}", flush=True)
        try:
            from python.train.outerloop.modal_job import vol
            vol.commit()
        except Exception:
            pass

        # Early stopping on policy accuracy
        if early_stop_patience > 0:
            val_pacc = val_metrics["policy_accuracy"]
            if val_pacc > best_val_pacc:
                best_val_pacc = val_pacc
                epochs_without_improvement = 0
            else:
                epochs_without_improvement += 1
                if epochs_without_improvement >= early_stop_patience:
                    print(
                        f"[distill] Early stopping at epoch {epoch+1} — "
                        f"no improvement in {early_stop_patience} epochs (best pacc={best_val_pacc:.4f})",
                        flush=True,
                    )
                    break

    validation = evaluate(model, valid_loader, device)
    output_dir = Path(spec["output_dir"])
    output_dir.mkdir(parents=True, exist_ok=True)
    model_path = output_dir / "hybrid_model.pt"
    torch.save(model.state_dict(), model_path)
    training_config = {
        "input_channels": int(grid_shape[0]),
        "scalar_features": int(scalars_shape[0]),
        "board_height": int(grid_shape[1]),
        "board_width": int(grid_shape[2]),
        "conv_channels": int(spec.get("conv_channels", 8)),
        "num_conv_layers": int(spec.get("num_conv_layers", 2)),
    }
    training_config_path = output_dir / "training_config.json"
    training_config_path.write_text(json.dumps(training_config, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    metrics = {
        "mode": "distill",
        "device": str(device),
        "samples": len(dataset),
        "train_samples": len(train_indices),
        "valid_samples": len(valid_indices),
        "distill_temperature": T,
        "distill_alpha": alpha,
        "train_total_loss": train_total_loss,
        "validation_value_mae": validation["value_mae"],
        "validation_value_correlation": validation["value_correlation"],
        "validation_policy_accuracy": validation["policy_accuracy"],
        "training_wallclock_minutes": (time.time() - started) / 60.0,
        "model_path": str(model_path),
        "training_config_path": str(training_config_path),
    }
    metrics_path = output_dir / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return metrics


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset-path", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--device-preference", type=str, default="mps")
    parser.add_argument("--epochs", type=int, default=4)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-4)
    parser.add_argument("--train-split", type=float, default=0.9)
    parser.add_argument("--max-samples", type=int, default=0)
    parser.add_argument("--conv-channels", type=int, default=8)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--num-conv-layers", type=int, default=2)
    parser.add_argument("--policy-loss-weight", type=float, default=1.0)
    args = parser.parse_args()
    metrics = train_from_spec(vars(args))
    print(json.dumps(metrics, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
