from __future__ import annotations

import json
from glob import glob
from pathlib import Path

import torch
from torch.utils.data import Dataset

class SelfPlayDataset(Dataset[dict[str, torch.Tensor]]):
    """Loads JSONL samples exported from the Rust self-play pipeline.

    Expected schema per line:
    {
      "grid": [[[...]]],      # [channels][height][width]
      "scalars": [...],       # scalar feature vector
      "value": 0.12,          # normalized final score target in [-1, 1]
      "weight": 1.0           # optional sample weight
    }
    """

    def __init__(self, path: str | Path, max_samples: int = 0) -> None:
        self.path = Path(path)
        self.paths = resolve_dataset_paths(path)
        if not self.paths:
            raise FileNotFoundError(f"dataset not found: {path}")

        self.rows: list[dict] = []
        for dataset_path in self.paths:
            with dataset_path.open("r", encoding="utf-8") as handle:
                for line in handle:
                    line = line.strip()
                    if not line:
                        continue
                    self.rows.append(json.loads(line))
                    if max_samples and len(self.rows) >= max_samples:
                        break
            if max_samples and len(self.rows) >= max_samples:
                break

        if not self.rows:
            raise ValueError(f"dataset is empty: {path}")

    def __len__(self) -> int:
        return len(self.rows)

    def __getitem__(self, index: int) -> dict[str, torch.Tensor]:
        row = self.rows[index]
        return {
            "grid": torch.tensor(row["grid"], dtype=torch.float32),
            "scalars": torch.tensor(row.get("scalars", []), dtype=torch.float32),
            "value": torch.tensor([row["value"]], dtype=torch.float32),
            "weight": torch.tensor([row.get("weight", 1.0)], dtype=torch.float32),
        }


def resolve_dataset_paths(path: str | Path) -> list[Path]:
    raw = str(path)
    dataset_path = Path(raw)
    if any(token in raw for token in "*?["):
        return sorted(Path(match) for match in glob(raw))
    if dataset_path.is_dir():
        return sorted(dataset_path.glob("*.jsonl"))
    if dataset_path.exists():
        return [dataset_path]
    raise FileNotFoundError(f"dataset not found: {dataset_path}")
