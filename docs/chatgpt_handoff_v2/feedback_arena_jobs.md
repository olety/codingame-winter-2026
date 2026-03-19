---
name: arena parallelism timing
description: Local arena evaluation with high job count inflates p99 timing — always use --jobs 2 for authoritative runs
type: feedback
---

Always run local authoritative arena evaluations with `--jobs 2` (not the default which uses all CPUs minus 2).

**Why:** With 14 parallel jobs, CPU contention inflated p99 from 40ms to 75ms, causing the timing gate (45ms) to falsely reject C8 — a candidate that was actually +0.94 on heldout. The incumbent also showed 73ms p99 under contention, proving the measurement was an artifact. Re-running with 2 jobs gave accurate 40ms p99 and changed the result from rejected to accepted.

**How to apply:** Whenever running `python3 -m python.train.run_arena` for stage-2 authoritative evaluation, always pass `--jobs 2`. Screening (stage-1) via Modal is fine since it runs isolated. The competition server runs single games, so low-contention measurement is the realistic scenario.
