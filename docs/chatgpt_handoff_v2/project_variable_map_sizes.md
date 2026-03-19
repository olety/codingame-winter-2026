---
name: variable map sizes and padding
description: Game has variable board sizes (height 10-24, width=round(h*1.8) even) — must zero-pad to (19,24,44) in training and inference
type: project
---

## Variable Map Sizes (2026-03-14)

### Source (upstream referee `GridMaker.java`)
- `MIN_GRID_HEIGHT = 10`, `MAX_GRID_HEIGHT = 24`
- `ASPECT_RATIO = 1.8f`
- `width = round(height * 1.8)`, forced even
- Height distribution skewed by league: Legend `skew=0.3` → biased toward larger maps

### Observed in self-play data
- `(19, 22, 40)`: 245,191 samples (56%)
- `(19, 23, 42)`: 189,346 samples (44%)
- Other sizes possible but rare due to skew

### Fix implemented
- **Python dataset**: `_pad_grid()` in `python/train/outerloop/dataset.py` zero-pads to `(19, 24, 44)`
- **Rust inference**: `pad_grid()` in `rust/bot/src/hybrid.rs` pads encoded grid before `model.forward()`
- **Constants**: `MAX_BOARD_HEIGHT=24`, `MAX_BOARD_WIDTH=44` exported from `features.rs`
- Models use `AdaptiveAvgPool2d(1)` / global average pool — handles any spatial size

**Why:** Without padding, DataLoader crashes on mixed-size batches. Without matching pad in Rust, the global average pool denominator differs from training, shifting feature magnitudes.
**How to apply:** All training and inference must pad to (19, 24, 44). Never filter by shape — use all map sizes.
