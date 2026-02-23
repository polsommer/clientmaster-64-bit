import asyncio
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool.loadtest import LoadTestScenario, run_loadtest


def test_loadtest_manifest(tmp_path: Path) -> None:
    manifest = {
        "client_path": sys.executable,
        "clients": 1,
        "working_directory": ".",
        "launch_rate_per_minute": 120,
        "shard": {"host": "127.0.0.1", "port": 44453},
        "env": {"SOME_FLAG": "1"},
        "arguments": ["--version"],
        "wrapper": ["python"],
    }
    manifest_path = tmp_path / "manifest.json"
    manifest_path.write_text(json.dumps(manifest))

    scenario = LoadTestScenario.from_manifest(manifest_path)

    assert scenario.client_path == Path(sys.executable)
    assert scenario.working_directory == manifest_path.parent
    assert scenario.wrapper == ["python"]
    assert scenario.clients == 1
    assert scenario.env["SOME_FLAG"] == "1"
    assert scenario.shard_host == "127.0.0.1"
    assert scenario.shard_port == 44453
    assert scenario.arguments == ["--version"]


def test_run_loadtest(tmp_path: Path) -> None:
    scenario = LoadTestScenario(
        client_path=Path(sys.executable),
        working_directory=Path(sys.executable).parent,
        wrapper=[],
        clients=2,
        launch_rate_per_minute=120,
        shard_host="localhost",
        shard_port=44453,
        env={"PYTHONUNBUFFERED": "1"},
        arguments=["-c", "import time; print('ok');"],
        timeout_seconds=5,
        max_retries=0,
    )

    report = asyncio.run(run_loadtest(scenario, tmp_path / "logs"))

    assert report["successful_clients"] == 2
    assert report["failed_clients"] == 0
    assert len(report["runs"]) == 2
    assert report["median_login_duration_seconds"] is not None
