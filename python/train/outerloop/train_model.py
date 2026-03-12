from __future__ import annotations

import argparse
import json
import math
import random
import time
from pathlib import Path

import torch
from torch import nn
from torch.utils.data import DataLoader, Subset

from python.train.outerloop.dataset import HybridSelfPlayDataset, dedup_rows, grouped_split_indices
from python.train.outerloop.model import TinyHybridNet


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
    ).to(device)
    optimizer = torch.optim.AdamW(
        model.parameters(),
        lr=float(spec.get("learning_rate", 1e-3)),
        weight_decay=float(spec.get("weight_decay", 1e-4)),
    )
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
    parser.add_argument("--policy-loss-weight", type=float, default=1.0)
    args = parser.parse_args()
    metrics = train_from_spec(vars(args))
    print(json.dumps(metrics, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
