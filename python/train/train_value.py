from __future__ import annotations

import argparse
import json
import math
import random
import time
from collections import defaultdict
from pathlib import Path

import torch
from torch import nn
from torch.utils.data import DataLoader, Subset

from python.train.dataset import SelfPlayDataset
from python.train.experiment import EXPERIMENT
from python.train.model import ValueNet


def select_device(preference: str) -> torch.device:
    if preference == "mps" and torch.backends.mps.is_available():
        return torch.device("mps")
    if preference == "cuda" and torch.cuda.is_available():
        return torch.device("cuda")
    return torch.device("cpu")


def set_seed(seed: int) -> None:
    random.seed(seed)
    torch.manual_seed(seed)


def evaluate(model: nn.Module, loader: DataLoader, device: torch.device) -> tuple[float, float]:
    model.eval()
    mae_total = 0.0
    preds: list[float] = []
    targets: list[float] = []
    with torch.no_grad():
        for batch in loader:
            grid = batch["grid"].to(device)
            scalars = batch["scalars"].to(device)
            target = batch["value"].to(device)
            pred = model(grid, scalars)
            mae_total += torch.abs(pred - target).mean().item()
            preds.extend(pred.squeeze(1).cpu().tolist())
            targets.extend(target.squeeze(1).cpu().tolist())
    mae = mae_total / max(len(loader), 1)
    corr = pearson(preds, targets)
    return mae, corr


def pearson(preds: list[float], targets: list[float]) -> float:
    if len(preds) < 2:
        return 0.0
    pred_mean = sum(preds) / len(preds)
    targ_mean = sum(targets) / len(targets)
    num = sum((p - pred_mean) * (t - targ_mean) for p, t in zip(preds, targets))
    pred_den = math.sqrt(sum((p - pred_mean) ** 2 for p in preds))
    targ_den = math.sqrt(sum((t - targ_mean) ** 2 for t in targets))
    denom = pred_den * targ_den
    if denom == 0:
        return 0.0
    return num / denom


def dedup_rows(rows: list[dict]) -> list[dict]:
    deduped: list[dict] = []
    seen: set[str] = set()
    for row in rows:
        key = str(row.get("encoded_view_hash", f"row-{len(deduped)}"))
        if key in seen:
            continue
        seen.add(key)
        deduped.append(row)
    return deduped


def grouped_split_indices(rows: list[dict], train_split: float, seed: int) -> tuple[list[int], list[int]]:
    grouped: dict[tuple[int, str], list[int]] = defaultdict(list)
    for index, row in enumerate(rows):
        grouped[(int(row.get("seed", 0)), str(row.get("game_id", "game-0")))].append(index)

    groups = list(grouped.items())
    rng = random.Random(seed)
    rng.shuffle(groups)

    target_train = max(1, int(len(rows) * train_split))
    train_indices: list[int] = []
    valid_indices: list[int] = []

    for _, indices in groups:
        bucket = train_indices if len(train_indices) < target_train else valid_indices
        bucket.extend(indices)

    if not valid_indices and train_indices:
        valid_indices.append(train_indices.pop())
    if not train_indices and valid_indices:
        train_indices.append(valid_indices.pop())

    return train_indices, valid_indices


def zero_baseline_mae(loader: DataLoader, device: torch.device) -> float:
    total = 0.0
    count = 0
    with torch.no_grad():
        for batch in loader:
            target = batch["value"].to(device)
            total += torch.abs(target).mean().item()
            count += 1
    return total / max(count, 1)


def train(config: dict, metrics_path: Path | None = None) -> dict:
    set_seed(int(config["seed"]))
    device = select_device(str(config["device_preference"]))
    started = time.time()

    dataset = SelfPlayDataset(config["dataset_path"], max_samples=int(config["max_samples"]))
    dataset.rows = dedup_rows(dataset.rows)
    train_indices, valid_indices = grouped_split_indices(
        dataset.rows,
        float(config["train_split"]),
        int(config["seed"]),
    )
    train_set = Subset(dataset, train_indices)
    valid_set = Subset(dataset, valid_indices)

    train_loader = DataLoader(train_set, batch_size=int(config["batch_size"]), shuffle=True)
    valid_loader = DataLoader(valid_set, batch_size=int(config["batch_size"]), shuffle=False)

    model = ValueNet(
        input_channels=int(config["input_channels"]),
        scalar_features=int(config["scalar_features"]),
        conv_channels=int(config["conv_channels"]),
        res_blocks=int(config["res_blocks"]),
    ).to(device)
    optimizer = torch.optim.AdamW(
        model.parameters(),
        lr=float(config["learning_rate"]),
        weight_decay=float(config["weight_decay"]),
    )
    loss_fn = nn.MSELoss(reduction="none")

    train_loss = 0.0
    for _ in range(int(config["epochs"])):
        model.train()
        for batch in train_loader:
            grid = batch["grid"].to(device)
            scalars = batch["scalars"].to(device)
            target = batch["value"].to(device)
            weight = batch["weight"].to(device)
            pred = model(grid, scalars)
            loss = (loss_fn(pred, target) * weight).mean()
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            train_loss = loss.item()

    validation_mae, validation_correlation = evaluate(model, valid_loader, device)
    baseline_mae = zero_baseline_mae(valid_loader, device)
    metrics = {
        "device": str(device),
        "samples": len(dataset.rows),
        "train_samples": len(train_indices),
        "valid_samples": len(valid_indices),
        "unique_games": len({(row.get("seed", 0), row.get("game_id", "game-0")) for row in dataset.rows}),
        "train_loss": train_loss,
        "validation_mae": validation_mae,
        "validation_zero_baseline_mae": baseline_mae,
        "validation_correlation": validation_correlation,
        "validation_score": 1.0 / (1.0 + validation_mae),
        "training_wallclock_minutes": (time.time() - started) / 60.0,
    }

    output_dir = Path(config["output_dir"])
    output_dir.mkdir(parents=True, exist_ok=True)
    torch.save(model.state_dict(), output_dir / "value_model.pt")
    metrics_path = metrics_path or (output_dir / "metrics.json")
    metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True), encoding="utf-8")
    return metrics


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics-path", type=Path, default=None)
    args = parser.parse_args()
    metrics = train(EXPERIMENT, metrics_path=args.metrics_path)
    print(json.dumps(metrics, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
