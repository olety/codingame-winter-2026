# Memory Index — WinterChallenge2026 Exotec

## THIS USER IS NOT RICH. EVERY DOLLAR OF GPU SPEND IS REAL MONEY OUT OF HIS POCKET. DO THE MATH BEFORE LAUNCHING ANYTHING. GET EXPLICIT APPROVAL WITH EXACT COST ESTIMATES. NO EXCEPTIONS. EVER.

## NEVER FABRICATE NUMBERS, TIMING DATA, OR CLAIMS ABOUT CODE YOU HAVEN'T READ. THIS HAS COST REAL MONEY AND TIME MULTIPLE TIMES. IF YOU DON'T KNOW, SAY "I DON'T KNOW." SEE feedback_never_fabricate.md.

## SEARCH IS MAXIMIN, NOT MCTS. SELF-PLAY LABELS ARE MAXIMIN LABELS. READ THE CODE BEFORE USING TECHNICAL TERMS.

- [feedback_never_fabricate.md](feedback_never_fabricate.md) — CRITICAL: never make up numbers, timing data, or claims about unread code
- [feedback_cost_awareness.md](feedback_cost_awareness.md) — CRITICAL: always estimate+confirm costs before GPU jobs, memory is hidden cost
- [feedback_be_proactive.md](feedback_be_proactive.md) — Stop being passive/defeatist, run the obvious test, solve problems immediately
- [feedback_distill_logging.md](feedback_distill_logging.md) — NEVER launch long GPU jobs without per-epoch logging and checkpointing
- [feedback_mmap_training.md](feedback_mmap_training.md) — Mmap works for training even when dataset >> RAM, don't default to subsetting
- [project_puct_attempts.md](project_puct_attempts.md) — PUCT search history: what worked, what failed, current short-bird problem
- [feedback_arena_jobs.md](feedback_arena_jobs.md) — Arena evaluation must use low parallelism for accurate timing
- [feedback_r2_mmap_perf.md](feedback_r2_mmap_perf.md) — R2 CloudBucketMount memmap extremely slow for random access, must preload into RAM
- [feedback_torch_compile_keys.md](feedback_torch_compile_keys.md) — torch.compile checkpoints have _orig_mod. prefix, strip when loading
- [feedback_training_optim.md](feedback_training_optim.md) — Training optimization lessons: channels_last conflicts with Muon, native Muon only 2D
- [feedback_vastai_gotchas.md](feedback_vastai_gotchas.md) — Vast.ai gotchas: output parsing, VRAM filtering, repo URL, onstart limits
- [feedback_remote_selfplay.md](feedback_remote_selfplay.md) — Remote selfplay: never use CC bg jobs for SSH, kill parent before children, verify process counts
- [feedback_vastai_volumes.md](feedback_vastai_volumes.md) — Use vast.ai volumes to avoid re-downloading 93GB dataset per instance
- [project_distill03_status.md](project_distill03_status.md) — Distill03 state: 96ch training running on M4 Max, Mac Mini syncing, C bot ready
- [project_leaderboard.md](project_leaderboard.md) — Leaderboard history and submission tracking
- [project_cg_timing.md](project_cg_timing.md) — CG inference timing — ONLY trust measured numbers, 96ch NOT YET MEASURED
- [project_submission_encoding.md](project_submission_encoding.md) — CodinGame 100K char limit, base64->Unicode->f16 encoding, character budget math
- [project_infra_compute.md](project_infra_compute.md) — Full machine specs, SSH, shells, storage, training capabilities
- [project_selfplay_gen.md](project_selfplay_gen.md) — Distributed self-play: 4 machines, 164K+ seeds, non-overlapping ranges, R2 push pipeline
- [project_modal_gpu_pricing.md](project_modal_gpu_pricing.md) — Modal GPU pricing (March 2026), A100 40GB is sweet spot at $2.10/hr
- [project_bitpack_pipeline.md](project_bitpack_pipeline.md) — Bitpacked pipeline: format spec, R2 layout, vast.ai vs Modal costs, current status
- [project_distill01_results.md](project_distill01_results.md) — First successful distillation run results and key learnings
- [project_distill02_results.md](project_distill02_results.md) — Distill02 results: student metrics, arena outcomes, quantization tests, character budget analysis
- [project_muon_ab_results.md](project_muon_ab_results.md) — Muon wins A/B test: +10.9% val_corr, +10.3% policy_acc over AdamW on 128ch teacher
- [project_variable_map_sizes.md](project_variable_map_sizes.md) — Game has variable board sizes (10-24 height), must zero-pad to (19,24,44)
- [project_plan_v2.md](project_plan_v2.md) — Scaling plan: 20K seeds, 512ch Muon teacher, dual Rust/Python students (Phase B done)
- [project_sweep_results.md](project_sweep_results.md) — 128ch sweep: wd=1e-3 wins, lr=0.005 best LR, BS test in progress
- [project_autotune_results.md](project_autotune_results.md) — torch.compile autotune kernel results for 128ch teacher on H100, compile time optimization tips
- [project_continued_training.md](project_continued_training.md) — Checkpoint format saves full state, enabling warm-restart continued training with new data
- [reference_modal_cloudbucket.md](reference_modal_cloudbucket.md) — Modal CloudBucketMount can natively mount R2/S3 buckets as read-only filesystem in containers
- [reference_r2_creds.md](reference_r2_creds.md) — R2 credentials in skate, vast.ai API key, Modal secret locations
- [reference_modal_logs.md](reference_modal_logs.md) — How to get live logs from Modal (`modal app logs`) and vast.ai
