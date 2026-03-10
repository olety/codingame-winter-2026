from __future__ import annotations

import argparse
import json
import math
import random
import time
from pathlib import Path

import torch
from torch import nn
from torch.utils.data import DataLoader, random_split

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


def train(config: dict, metrics_path: Path | None = None) -> dict:
    set_seed(int(config["seed"]))
    device = select_device(str(config["device_preference"]))
    started = time.time()

    dataset = SelfPlayDataset(config["dataset_path"], max_samples=int(config["max_samples"]))
    train_len = max(1, int(len(dataset) * float(config["train_split"])))
    valid_len = max(1, len(dataset) - train_len)
    if train_len + valid_len > len(dataset):
        train_len -= 1
    train_set, valid_set = random_split(dataset, [train_len, len(dataset) - train_len])

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
    metrics = {
        "device": str(device),
        "samples": len(dataset),
        "train_loss": train_loss,
        "validation_mae": validation_mae,
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
