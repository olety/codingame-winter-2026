from __future__ import annotations

from pathlib import Path

from python.train.outerloop.registry import load_json


def rank_key(stage2_payload: dict) -> tuple[float, float, float, float]:
    result = stage2_payload.get("result", {})
    metrics = result.get("metrics", {})
    return (
        float(metrics.get("heldout_body_diff", 0.0)),
        float(metrics.get("heldout_win_margin", 0.0)),
        float(metrics.get("heldout_tiebreak_win_rate", 0.0)),
        -float(metrics.get("later_turn_p99_ms", 10_000.0)),
    )


def select_best(stage2_paths: list[str | Path]) -> dict | None:
    payloads = [load_json(path) for path in stage2_paths]
    accepted = [payload for payload in payloads if payload.get("promotable")]
    if not accepted:
        return None
    return max(accepted, key=rank_key)
