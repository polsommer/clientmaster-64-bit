"""Async load test runner for spawning SWG clients under supervision."""

from __future__ import annotations

import asyncio
import json
import os
import statistics
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional


@dataclass
class LoadTestScenario:
    """Configuration for a load test run."""

    client_path: Path
    clients: int
    launch_rate_per_minute: float
    shard_host: str
    shard_port: int
    working_directory: Optional[Path] = None
    wrapper: List[str] = field(default_factory=list)
    env: Dict[str, str] = field(default_factory=dict)
    arguments: List[str] = field(default_factory=list)
    timeout_seconds: float = 300.0
    max_retries: int = 0

    @classmethod
    def from_manifest(cls, manifest_path: Path) -> "LoadTestScenario":
        manifest_raw = json.loads(manifest_path.read_text())
        base_dir = manifest_path.parent

        def _require(key: str) -> Any:
            if key not in manifest_raw:
                raise ValueError(f"Manifest missing required field '{key}'")
            return manifest_raw[key]

        client_path = Path(_require("client_path")).expanduser()
        if not client_path.is_absolute():
            client_path = (base_dir / client_path).resolve()

        working_directory: Optional[Path] = None
        if "working_directory" in manifest_raw:
            working_directory = Path(manifest_raw["working_directory"]).expanduser()
            if not working_directory.is_absolute():
                working_directory = (base_dir / working_directory).resolve()
            if not working_directory.exists():
                raise ValueError(f"Working directory does not exist: {working_directory}")
        else:
            working_directory = client_path.parent

        clients = int(_require("clients"))
        launch_rate = float(_require("launch_rate_per_minute"))
        shard = manifest_raw.get("shard", {})
        shard_host = shard.get("host")
        shard_port = shard.get("port")
        if shard_host is None or shard_port is None:
            raise ValueError("Manifest shard.host and shard.port are required")

        env = {str(key): str(value) for key, value in manifest_raw.get("env", {}).items()}
        wrapper = [str(value) for value in manifest_raw.get("wrapper", [])]
        arguments = [str(value) for value in manifest_raw.get("arguments", [])]

        timeout_seconds = float(manifest_raw.get("timeout_seconds", 300))
        max_retries = int(manifest_raw.get("max_retries", 0))

        return cls(
            client_path=client_path,
            working_directory=working_directory,
            wrapper=wrapper,
            clients=clients,
            launch_rate_per_minute=launch_rate,
            shard_host=str(shard_host),
            shard_port=int(shard_port),
            env=env,
            arguments=arguments,
            timeout_seconds=timeout_seconds,
            max_retries=max_retries,
        )


def _build_environment(scenario: LoadTestScenario) -> Dict[str, str]:
    env = os.environ.copy()
    env.update(scenario.env)
    env.setdefault("SWG_SHARD_HOST", scenario.shard_host)
    env.setdefault("SWG_SHARD_PORT", str(scenario.shard_port))
    return env


async def _pipe_stream(stream: asyncio.StreamReader, log_file, prefix: str) -> None:
    while True:
        chunk = await stream.readline()
        if not chunk:
            break
        timestamp = datetime.now(timezone.utc).isoformat()
        log_file.write(f"[{timestamp}] {prefix}: {chunk.decode(errors='replace')}")
        log_file.flush()


async def _run_client(
    index: int, scenario: LoadTestScenario, log_directory: Path, launch_delay: float
) -> List[Dict[str, Any]]:
    await asyncio.sleep(max(launch_delay, 0))
    attempts: List[Dict[str, Any]] = []

    for attempt in range(1, scenario.max_retries + 2):
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        log_path = log_directory / f"client_{index}_attempt_{attempt}_{timestamp}.log"
        log_directory.mkdir(parents=True, exist_ok=True)

        start_time = datetime.now(timezone.utc)
        env = _build_environment(scenario)

        command = [*scenario.wrapper, str(scenario.client_path), *scenario.arguments]

        process = await asyncio.create_subprocess_exec(
            *command,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            env=env,
            cwd=str(scenario.working_directory) if scenario.working_directory else None,
        )

        stdout_task: Optional[asyncio.Task[None]] = None
        stderr_task: Optional[asyncio.Task[None]] = None
        with log_path.open("w", encoding="utf-8") as log_file:
            if process.stdout:
                stdout_task = asyncio.create_task(_pipe_stream(process.stdout, log_file, "stdout"))
            if process.stderr:
                stderr_task = asyncio.create_task(_pipe_stream(process.stderr, log_file, "stderr"))

            timed_out = False
            try:
                await asyncio.wait_for(process.wait(), timeout=scenario.timeout_seconds)
            except asyncio.TimeoutError:
                timed_out = True
                process.kill()
                await process.wait()

            if stdout_task:
                await stdout_task
            if stderr_task:
                await stderr_task

        end_time = datetime.now(timezone.utc)
        duration = (end_time - start_time).total_seconds()

        success = not timed_out and process.returncode == 0
        attempts.append(
            {
                "client_index": index,
                "attempt": attempt,
                "log": str(log_path),
                "start_time": start_time.isoformat(),
                "end_time": end_time.isoformat(),
                "duration_seconds": duration,
                "return_code": process.returncode,
                "timed_out": timed_out,
                "success": success,
            }
        )

        if success:
            break

    return attempts


async def run_loadtest(scenario: LoadTestScenario, log_directory: Path) -> Dict[str, Any]:
    start_time = datetime.now(timezone.utc)
    interval = 60.0 / scenario.launch_rate_per_minute if scenario.launch_rate_per_minute > 0 else 0

    tasks = [
        asyncio.create_task(
            _run_client(index=i, scenario=scenario, log_directory=log_directory, launch_delay=i * interval)
        )
        for i in range(scenario.clients)
    ]

    runs_nested = await asyncio.gather(*tasks)
    runs = [attempt for client_runs in runs_nested for attempt in client_runs]

    successful_runs = [run for run in runs if run["success"]]
    failed_runs = [run for run in runs if not run["success"]]
    durations = [run["duration_seconds"] for run in successful_runs]

    end_time = datetime.now(timezone.utc)

    report = {
        "start_time": start_time.isoformat(),
        "end_time": end_time.isoformat(),
        "duration_seconds": (end_time - start_time).total_seconds(),
        "total_clients": scenario.clients,
        "attempts": len(runs),
        "successful_clients": len(successful_runs),
        "failed_clients": len(failed_runs),
        "median_login_duration_seconds": statistics.median(durations) if durations else None,
        "runs": runs,
    }

    return report


async def run_loadtest_from_manifest(manifest_path: Path, log_directory: Path) -> Dict[str, Any]:
    scenario = LoadTestScenario.from_manifest(manifest_path)
    return await run_loadtest(scenario, log_directory)


def run_loadtest_sync(manifest_path: Path, log_directory: Path) -> Dict[str, Any]:
    return asyncio.run(run_loadtest_from_manifest(manifest_path, log_directory))


__all__ = [
    "LoadTestScenario",
    "run_loadtest",
    "run_loadtest_from_manifest",
    "run_loadtest_sync",
]
