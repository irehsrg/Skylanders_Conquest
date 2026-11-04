#!/usr/bin/env python3
"""Run a headless Skylanders Conquest match and report the smoke test result.

Launches UnrealEditor-Cmd on the Joust map with -nullrhi and -SkySmoke=<seconds>
(see Source/Skylanders_Conquest/SkylandersSmokeTest.h), waits for it to exit,
prints a compact summary of Saved/SmokeTest/report.json plus the last [Smoke]
log lines, and exits with the game's exit code (0 = pass).

    python Scripts/smoke_test.py               # 120 s match
    python Scripts/smoke_test.py --seconds 60
"""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ENGINE_CMD = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
PROJECT_DIR = Path(__file__).resolve().parent.parent
UPROJECT = PROJECT_DIR / "Skylanders_Conquest.uproject"
REPORT = PROJECT_DIR / "Saved" / "SmokeTest" / "report.json"
LOG = PROJECT_DIR / "Saved" / "Logs" / "smoke.log"
STARTUP_GRACE_S = 180  # engine boot + shader/asset load before the match clock starts


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seconds", type=int, default=120, help="match length to simulate (default 120)")
    ap.add_argument("--map", default="Joust", help="map to load (default Joust)")
    args = ap.parse_args()

    if REPORT.exists():
        REPORT.unlink()  # never report a stale result

    cmd = [str(ENGINE_CMD), str(UPROJECT), args.map, "-game", "-nullrhi", "-unattended", "-nosound",
           "-nosplash", "-NoLiveCoding", f"-SkySmoke={args.seconds}", "-log=smoke.log"]
    limit = args.seconds + STARTUP_GRACE_S
    print(f"[smoke] running {args.map} headless for {args.seconds} s (timeout {limit} s)...", flush=True)
    start = time.monotonic()
    proc = subprocess.Popen(cmd, cwd=str(PROJECT_DIR), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        code = proc.wait(timeout=limit)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        print(f"[smoke] TIMEOUT after {limit} s: process killed")
        code = 124
    print(f"[smoke] exit={code} after {time.monotonic() - start:.0f} s")

    if REPORT.exists():
        try:
            r = json.loads(REPORT.read_text(encoding="utf-8"))
            print(f"passed:   {r.get('passed')}")
            fails = r.get("failures") or []
            print(f"failures: {'none' if not fails else ''}")
            for f in fails:
                print(f"  - {f}")
            counters = r.get("counters") or {}
            print("counters: " + ", ".join(f"{k}={v}" for k, v in sorted(counters.items())))
            errs = r.get("errors") or []
            print(f"errors:   {r.get('error_count', len(errs))}")
            for e in errs:
                print(f"  - {e}")
            samples = r.get("samples") or []
            if samples:
                print(f"last sample: {json.dumps(samples[-1])}")
        except (OSError, ValueError) as e:
            print(f"[smoke] could not read report: {e}")
    else:
        print(f"[smoke] no report at {REPORT}")

    if LOG.exists():
        lines = [l.rstrip() for l in LOG.read_text(encoding="utf-8", errors="replace").splitlines() if "[Smoke]" in l]
        if lines:
            print("log:")
            for l in lines[-5:]:
                print("  " + l)

    if code == 0 and not REPORT.exists():
        return 1  # exited cleanly without running the test: treat as failure
    return code


if __name__ == "__main__":
    sys.exit(main())
