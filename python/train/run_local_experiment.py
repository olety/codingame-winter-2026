from __future__ import annotations

import json
from pathlib import Path

from python.train.experiment import EXPERIMENT
from python.train.results import append_result, check_gates, compute_composite
from python.train.train_value import train


def main() -> None:
    metrics = train(EXPERIMENT)
    gate_result = check_gates(metrics)
    status = "accepted" if gate_result.passed else "rejected"
    append_result(
        EXPERIMENT["results_db"],
        name=EXPERIMENT["name"],
        status=status,
        description="local supervised training run",
        metrics=metrics,
        failures=gate_result.failures,
    )
    payload = {
        "status": status,
        "composite_score": compute_composite(metrics),
        "failures": gate_result.failures,
        "metrics": metrics,
    }
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
