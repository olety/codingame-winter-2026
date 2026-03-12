from __future__ import annotations

from copy import deepcopy

from python.train.outerloop.genome import CandidateGenome, normalize_genome


def crossover(left: CandidateGenome, right: CandidateGenome) -> CandidateGenome:
    payload = deepcopy(left.payload)
    payload["search"]["deepen_top_my"] = right.search["deepen_top_my"]
    payload["search"]["deepen_top_opp"] = right.search["deepen_top_opp"]
    payload["search"]["deepen_child_my"] = left.search["deepen_child_my"]
    payload["search"]["deepen_child_opp"] = left.search["deepen_child_opp"]
    payload["search"]["later_turn_ms"] = min(
        left.search["later_turn_ms"],
        right.search["later_turn_ms"],
    )
    payload["eval"] = deepcopy(right.eval)
    payload["model"] = deepcopy(right.model)
    payload["data"] = deepcopy(left.data)
    payload["metadata"]["parents"] = [left.semantic_hash, right.semantic_hash]
    return CandidateGenome(normalize_genome(payload))
