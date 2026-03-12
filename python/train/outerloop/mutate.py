from __future__ import annotations

import random
from copy import deepcopy

from python.train.outerloop.genome import CandidateGenome, normalize_genome


SEARCH_RANGES = {
    "deepen_top_my": (4, 9),
    "deepen_top_opp": (4, 10),
    "deepen_child_my": (2, 6),
    "deepen_child_opp": (2, 6),
    "later_turn_ms": (36, 44),
    "first_turn_ms": (800, 950),
    "extra_nodes_after_root": (1000, 8000),
}


def mutate_genome(genome: CandidateGenome, *, seed: int | None = None) -> CandidateGenome:
    rng = random.Random(seed)
    payload = deepcopy(genome.payload)
    search_key = rng.choice(list(SEARCH_RANGES))
    lo, hi = SEARCH_RANGES[search_key]
    if isinstance(lo, int) and isinstance(hi, int):
        payload["search"][search_key] = rng.randint(lo, hi)
    payload["metadata"]["mutated_from"] = genome.semantic_hash
    return CandidateGenome(normalize_genome(payload))
