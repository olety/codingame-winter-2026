# 2026-03-12 Prose Outer Loop and Hybrid Lab

## What was added

The repo now has the first real Prose-first research lab scaffold.

Control plane:

- `automation/outerloop/program.md`
- `automation/outerloop/outerloop.prose`
- focused subprograms for offspring generation, screening, hybrid training, authoritative evaluation, and promotion

Worker/runtime layer:

- `python/train/outerloop/genome.py`
- `python/train/outerloop/registry.py`
- `python/train/outerloop/workspace.py`
- `python/train/outerloop/run_candidate.py`
- `python/train/outerloop/train_model.py`
- `python/train/outerloop/export_weights.py`
- `python/train/outerloop/build_dataset.py`
- `python/train/outerloop/launch_modal.py`
- `python/train/outerloop/modal_job.py`
- `python/train/outerloop/promote.py`

Rust hybrid hooks:

- `rust/bot/src/hybrid.rs`
- hybrid config support in `rust/bot/src/config.rs`
- hybrid-guided ordering / leaf blending in `rust/bot/src/search.rs`
- identity-preserving feature export in `rust/bot/src/features.rs`

## Architecture split

The intended ownership is now:

- Prose: workflow logic
- Python/Rust CLIs: deterministic work
- Helios: optional external cockpit/UI

This is deliberate. Helios is not supposed to own selection, mutation, arena logic, or promotion semantics. It should read the repo artifacts and drive the repo CLIs.

## Candidate artifacts

The outer loop now writes candidate artifacts under:

- `artifacts/outerloop/runs/<run_id>/manifest.json`
- `artifacts/outerloop/runs/<run_id>/candidates/<candidate_id>/`

The intended candidate directory contract is:

- `genome.json`
- `stage0.json`
- `stage1.json`
- `stage2.json` when authoritative evaluation runs
- optional `patch.diff`
- optional `model/`

Stage result JSONs now carry:

- `run_id`
- `candidate_id`
- `candidate_dir`
- `stage`
- `status`
- `artifact_hash`
- `behavior_hash`
- `genome_hash`
- `executor`
- `started_at`
- `finished_at`

The run manifest tracks:

- run id
- program path
- current status
- active stage
- candidate map
- promoted candidate id if one exists

## Hybrid branch

The hybrid branch is now present but still experimental.

Important current design:

- search remains the controller
- the tiny network is only for action ordering / value blending
- the export path now includes `hybrid_grid` and `policy_targets`
- the Python side can train a tiny conv net and export JSON weights
- the Rust side can load those weights and produce priors / leaf bonuses

Current representation:

- walls
- apples
- a support/fall-risk channel
- per-bird head/body channels for all four friendly and four enemy bots
- the existing six scalar features

## Verified so far

Verified locally in this phase:

- `cargo test --workspace`
- `python3 tools/generate_flattened_submission.py`
- small Rust-seed self-play export with the new hybrid schema
- tiny local hybrid train smoke on `mps`
- Rust-consumable weight export
- one `run_candidate` screening smoke that produced a real outer-loop run manifest and candidate stage artifacts

The hybrid screening smoke was intentionally bad in strength/timing terms. That is not a regression result to optimize around; it only proves the candidate runner and artifact contracts work end to end.

## Native Prose VM smoke run

After the scaffold landed, the repo also got a real native Prose VM smoke run.

Run state:

- `.prose/runs/20260312-144527-prose01/`

Important files:

- `.prose/runs/20260312-144527-prose01/state.md`
- `.prose/runs/20260312-144527-prose01/bindings/offspring.md`
- `.prose/runs/20260312-144527-prose01/bindings/finalists.md`
- `.prose/runs/20260312-144527-prose01/bindings/verdict.md`

What the VM did:

1. interpreted `automation/outerloop/outerloop.prose` inside the agent session
2. spawned a planner session
3. spawned three worker screening sessions in parallel
4. spawned an evaluator session to pick one finalist
5. spawned one worker authoritative-eval session
6. spawned a final evaluator session to issue the verdict

The three offspring were:

- `search-37c207806429`
- `search-02d93fb0e8a4`
- `hybrid-c6941dcf7906`

The hybrid candidate behaved exactly like an early hybrid smoke should: extremely slow and weak. The two search candidates both screened positively on the tiny temporary suites, and the evaluator chose `search-02d93fb0e8a4` for one stage-2 rerun.

Stage-2 result on the tiny temporary suites:

- candidate: `search-02d93fb0e8a4`
- heldout body diff: `+5.0`
- shadow body diff: `+11.75`
- later-turn `p99`: `28 ms`
- Java smoke: passed

Final verdict:

- `promising_smoke_only`
- `promote: false`

Reason:

- the VM run was intentionally a tiny-suite runtime proof, not a real `heldout_v1` / `shadow_v1` promotion attempt
- the correct next step for that exact candidate is a normal authoritative rerun on the real frozen suites

This matters because it proves the Prose runtime model is actually viable in this repo:

- the session can act as the VM
- `.prose/runs/...` can hold durable execution state
- the existing repo CLIs are sufficient worker surfaces
- Helios can treat that filesystem state as UI data instead of reimplementing workflow logic

## Important limitation

The single-file CodinGame submission artifact is intentionally still search-only.

`tools/generate_flattened_submission.py` now fails explicitly if the chosen submission config enables the hybrid model. That is safer than pretending a hybrid submission is supported when the one-file generator does not yet embed weights.

So the current state is:

- local hybrid training/evaluation: supported
- hybrid search guidance in Rust: supported
- hybrid single-file CodinGame submission: not supported yet

## Dev ergonomics change

`workspace.py` now overlays the current repo’s uncommitted changes into candidate worktrees. That matters for local iteration because plain `git worktree add HEAD` would otherwise omit new outer-loop files until after a commit.

This is a development convenience, not a change to the authoritative promotion path.

## What to do next

The next high-value milestone is not “turn on RL.”

It is:

1. keep the current promoted search bot as the live baseline
2. use the new outer loop to search hybrid/search variants in a controlled way
3. only consider a hybrid submission path once a hybrid candidate is actually strong enough and the one-file embedding problem is solved
