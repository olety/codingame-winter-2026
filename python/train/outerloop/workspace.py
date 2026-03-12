from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]


def worktree_root(run_id: str) -> Path:
    return REPO_ROOT / "worktrees" / run_id


def _git_lines(*args: str) -> list[str]:
    output = subprocess.check_output(["git", *args], cwd=REPO_ROOT, text=True)
    return [line.strip() for line in output.splitlines() if line.strip()]


def _overlay_paths() -> list[Path]:
    changed = set(_git_lines("diff", "--name-only", "HEAD"))
    changed.update(_git_lines("diff", "--cached", "--name-only", "HEAD"))
    changed.update(_git_lines("ls-files", "--others", "--exclude-standard"))
    ignore_prefixes = (".serena/", "worktrees/", "target/")
    return [
        Path(rel)
        for rel in sorted(changed)
        if rel and not any(rel.startswith(prefix) for prefix in ignore_prefixes)
    ]


def _overlay_current_changes(worktree: Path) -> None:
    for rel_path in _overlay_paths():
        source = REPO_ROOT / rel_path
        target = worktree / rel_path
        if source.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        elif target.exists():
            if target.is_dir():
                shutil.rmtree(target)
            else:
                target.unlink()


def create_worktree(run_id: str, candidate_id: str, *, base_ref: str = "HEAD") -> Path:
    root = worktree_root(run_id)
    root.mkdir(parents=True, exist_ok=True)
    path = root / candidate_id
    subprocess.run(["git", "worktree", "prune"], cwd=REPO_ROOT, check=True)
    if path.exists():
        shutil.rmtree(path)
    subprocess.run(
        ["git", "worktree", "add", "--force", "--detach", str(path), base_ref],
        cwd=REPO_ROOT,
        check=True,
    )
    _overlay_current_changes(path)
    return path


def remove_worktree(path: str | Path) -> None:
    worktree = Path(path)
    if not worktree.exists():
        return
    subprocess.run(["git", "worktree", "remove", "--force", str(worktree)], cwd=REPO_ROOT, check=True)
