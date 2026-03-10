from __future__ import annotations

import json
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
        if not self.path.exists():
            raise FileNotFoundError(f"dataset not found: {self.path}")

        self.rows: list[dict] = []
        with self.path.open("r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                self.rows.append(json.loads(line))
                if max_samples and len(self.rows) >= max_samples:
                    break

        if not self.rows:
            raise ValueError(f"dataset is empty: {self.path}")

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
