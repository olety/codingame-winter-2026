# CodinGame Winter Challenge 2026 — Exotec

**Final result: Silver ~73rd-124th / 1,107 in league (508-578th / 2,161 global)**

A snake-like bird game bot for the [CodinGame Winter Challenge 2026](https://www.codingame.com/contests/winter-challenge-2026). The final submission is a pure C maximin heuristic bot — no neural network, no learned weights.

See [docs/RETROSPECTIVE.md](./docs/RETROSPECTIVE.md) for the full post-mortem, including reflections on the ML pipeline that didn't ship.

## Repository Layout

```
c_bot/           # Final C bot — maximin search + heuristic eval
  bot.c          # Main bot source (submitted as submission/c_bot_maximin.c)
rust/
  engine/        # Java-parity Rust game simulator
  bot/           # Rust contest bot (search + optional hybrid NN, superseded by C bot)
python/
  train/         # ML training pipeline (teacher, distillation, arena, self-play)
  bot/           # Pure-NN Python bot experiment (numpy inference)
submission/      # Submission artifacts (C, Rust, Python variants)
tools/           # Submission generators, sweep launchers, monitoring scripts
docs/            # Documentation and retrospective
config/          # Training configs and specs
```

## The Bot

### Architecture

- **Search**: Exhaustive 1-ply maximin over all my-actions x all opponent-actions
- **Pruning**: Alpha-beta cutoffs with heuristic action ordering
- **Deepening**: Top 6 x worst 8 opponent x 3x3 child branching
- **Scoring**: CVaR blend — `(1-lambda)*worst + lambda*cvar3`, lambda adaptive (0.05 ahead, 0.25 behind)
- **Time budget**: 950ms first turn, 45ms subsequent

### Evaluation Function

| Term | Weight | Description |
|------|--------|-------------|
| body | 120 | Raw body length difference |
| loss | 18 | Loss count difference |
| mobility | 7.5 | Legal commands per bird |
| apple | 60 | Competitive apple race: (opp_dist-my_dist)/(total+1) |
| coverage | 40 | Distinct-apple greedy matching |
| stability | 10 | Supported birds |
| breakpoint | 25 | Count of length>=4 birds difference |
| fragile | 8 | Proximity to opponent's short birds |
| danger | ~100 | Wall proximity (halved from initial) |
| gravity | ~40 | Unsupported/facing-up penalties (halved) |

## The ML Pipeline (built but not shipped)

The repo also contains a complete teacher-student distillation pipeline that was built during the competition but ultimately didn't improve on the heuristic bot:

1. **Distributed self-play** — 4 machines, 164K seeds, 36.8M positions, bitpacked binary format
2. **128ch/8-block SE-ResNet teacher** — Muon optimizer, val_corr=0.6728, policy_acc=90.32%
3. **Student distillation** — 12ch to 64ch variants, soft targets with T=2.0
4. **f16 Unicode encoding** — 1 char/weight (5.3x denser than base64), fits 32ch in 100K char limit
5. **CodinGame timing probes** — measured actual CNN inference on CG servers

Key finding: **policy priors hurt at <85% accuracy, and value-only integration was marginal (+0.94 body diff)**. Hand-tuned heuristics iterated faster and produced better results.

## Leaderboard History

| Date | Bot | League Rank | Global Rank |
|------|-----|-------------|-------------|
| Mar 11 | Rust search 6/8/3/3 | Bronze | 147/1,108 |
| Mar 12 | Same | Silver 108/763 | 108/1,388 |
| Mar 13 | Hybrid NN 12ch value-only | Silver 187/892 | 187/1,478 |
| Mar 19 | PUCT attempt (broken) | Silver 1,081/1,096 | 1,495/2,118 |
| Mar 19 | C maximin v1 | Silver 529/1,098 | 951/2,130 |
| Mar 19 | C maximin v2 (new eval) | Silver 367/1,102 | 790/2,137 |
| Mar 19 | C maximin v3b (CVaR+coverage) | Silver 106/1,099 | 532/2,138 |
| Mar 20 | v3b settled | **Silver 73/1,099** | **508/2,144** |
| Mar 20 | C maximin v6 (final) | Silver 124/1,107 | 578/2,161 |

## Cost

| Item | Cost |
|------|------|
| Modal GPU (teacher, distillation, sweeps) | ~$155 |
| Modal memory overcharges | ~$119 |
| Vast.ai (sweeps, zombie instances) | ~$15 |
| Local compute (self-play on 4 machines) | $0 |
| **Total** | **~$310** |

~$310 spent, mostly on ML infrastructure that didn't make it into the final submission. See the [retrospective](./docs/RETROSPECTIVE.md) for reflections.

## Git Remotes

- `origin`: `https://github.com/olety/codingame-winter-2026.git`
- `upstream`: `https://github.com/CodinGame/WinterChallenge2026-Exotec.git`
