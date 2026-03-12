from __future__ import annotations

import os
import subprocess
from pathlib import Path


def build_patch_prompt(*, allowed_files: list[str], failure_reason: str, genome_hash: str) -> str:
    return "\n".join(
        [
            "Generate a unified diff that only touches these files:",
            *[f"- {path}" for path in allowed_files],
            "",
            f"Candidate genome: {genome_hash}",
            f"Last failure: {failure_reason}",
            "",
            "Return only a valid patch.",
        ]
    )


def maybe_generate_patch(
    *,
    allowed_files: list[str],
    failure_reason: str,
    genome_hash: str,
    candidate_dir: Path,
) -> Path | None:
    command = os.environ.get("OUTERLOOP_PATCH_CMD")
    if not command:
        return None
    prompt = build_patch_prompt(
        allowed_files=allowed_files,
        failure_reason=failure_reason,
        genome_hash=genome_hash,
    )
    patch_path = candidate_dir / "patch.diff"
    output = subprocess.check_output(
        command,
        shell=True,
        text=True,
        input=prompt,
    )
    patch_path.write_text(output, encoding="utf-8")
    return patch_path


def apply_patch(worktree: Path, patch_path: Path) -> None:
    subprocess.run(["git", "apply", str(patch_path)], cwd=worktree, check=True)
