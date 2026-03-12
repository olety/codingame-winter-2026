from __future__ import annotations

import json
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from python.train.outerloop import OUTERLOOP_SCHEMA_VERSION

REPO_ROOT = Path(__file__).resolve().parents[3]


def iso_now() -> str:
    return datetime.now(UTC).isoformat(timespec="seconds")


def run_root(run_id: str) -> Path:
    return REPO_ROOT / "artifacts" / "outerloop" / "runs" / run_id


def ensure_run_manifest(run_id: str, *, program: str) -> Path:
    root = run_root(run_id)
    root.mkdir(parents=True, exist_ok=True)
    manifest_path = root / "manifest.json"
    if not manifest_path.exists():
        payload = {
            "schema_version": OUTERLOOP_SCHEMA_VERSION,
            "run_id": run_id,
            "program": program,
            "status": "created",
            "created_at": iso_now(),
            "updated_at": iso_now(),
            "active_stage": None,
            "promoted_candidate_id": None,
            "candidates": {},
        }
        write_json(manifest_path, payload)
    return manifest_path


def candidate_dir(run_id: str, candidate_id: str) -> Path:
    path = run_root(run_id) / "candidates" / candidate_id
    path.mkdir(parents=True, exist_ok=True)
    return path


def load_json(path: str | Path) -> dict[str, Any]:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write_json(path: str | Path, payload: dict[str, Any]) -> None:
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def register_candidate(
    run_id: str,
    candidate_id: str,
    *,
    genome_hash: str,
    behavior_hash: str | None = None,
    artifact_hash: str | None = None,
    kind: str,
) -> Path:
    manifest_path = ensure_run_manifest(run_id, program="automation/outerloop/outerloop.prose")
    manifest = load_json(manifest_path)
    candidate_path = candidate_dir(run_id, candidate_id)
    manifest["candidates"][candidate_id] = {
        "candidate_id": candidate_id,
        "candidate_dir": str(candidate_path),
        "genome_hash": genome_hash,
        "behavior_hash": behavior_hash,
        "artifact_hash": artifact_hash,
        "kind": kind,
        "status": "created",
        "stages": {},
        "updated_at": iso_now(),
    }
    manifest["updated_at"] = iso_now()
    write_json(manifest_path, manifest)
    return candidate_path


def write_stage_result(
    run_id: str,
    candidate_id: str,
    stage: str,
    payload: dict[str, Any],
) -> Path:
    manifest_path = ensure_run_manifest(run_id, program="automation/outerloop/outerloop.prose")
    manifest = load_json(manifest_path)
    candidate_path = candidate_dir(run_id, candidate_id)
    result_path = candidate_path / f"{stage}.json"
    write_json(result_path, payload)
    entry = manifest["candidates"].setdefault(candidate_id, {})
    entry["status"] = payload.get("status", "unknown")
    entry["updated_at"] = iso_now()
    entry.setdefault("stages", {})[stage] = str(result_path)
    entry["behavior_hash"] = payload.get("behavior_hash", entry.get("behavior_hash"))
    entry["artifact_hash"] = payload.get("artifact_hash", entry.get("artifact_hash"))
    manifest["active_stage"] = stage
    manifest["status"] = payload.get("status", manifest.get("status", "running"))
    manifest["updated_at"] = iso_now()
    write_json(manifest_path, manifest)
    return result_path


def mark_promoted(run_id: str, candidate_id: str) -> None:
    manifest_path = ensure_run_manifest(run_id, program="automation/outerloop/outerloop.prose")
    manifest = load_json(manifest_path)
    manifest["status"] = "promoted"
    manifest["promoted_candidate_id"] = candidate_id
    manifest["updated_at"] = iso_now()
    if candidate_id in manifest["candidates"]:
        manifest["candidates"][candidate_id]["status"] = "promoted"
    write_json(manifest_path, manifest)
