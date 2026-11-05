#!/usr/bin/env python3
"""Run a headless Skylanders Conquest match and report the smoke test result.

Launches UnrealEditor-Cmd on the Joust map with -nullrhi and -SkySmoke=<seconds>
(see Source/Skylanders_Conquest/SkylandersSmokeTest.h), waits for it to exit,
prints a compact summary of Saved/SmokeTest/report.json plus the last [Smoke]
log lines, and exits with the game's exit code (0 = pass).

    python Scripts/smoke_test.py                      # 120 s match as Trigger Happy
    python Scripts/smoke_test.py --seconds 60
    python Scripts/smoke_test.py --character Spyro    # play a specific kit
    python Scripts/smoke_test.py --all-characters     # every playable kit, back to back
    python Scripts/smoke_test.py --items              # + the item stat check (-SkyItemTest)
    python Scripts/smoke_test.py --ui                 # build every menu/end screen on MainMenu

--character passes -SkyCharacter=<ID> (USkylandersGameInstance::Init), using the
character-select IDs. --all-characters runs each kit for --seconds, prints one
summary line per character, and exits non-zero if any run failed. --items adds
-SkyItemTest: at the first sample the game checks every catalog item's stats,
the CDR caps, penetration math and the full-inventory refusal on the player,
restores it, and plays the match on; the report's "item_test" block is printed.
--ui loads the MainMenu map with -SkySmoke=10 -SkyUISmoke instead of a match: the
game creates the main menu, character select, shop, end screen and settings
widgets, runs their setup, lets a few frames pass, logs "[Smoke] ui <Class> ok"
for each and exits (0 = all five built without errors).
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
# Character-select IDs of every playable kit (FSkylanderSelectEntry::GetRoster)
ALL_CHARACTERS = ["TriggerHappy", "Hex", "TreeRex", "Spyro"]
SUMMARY_COUNTERS = ["player_shot", "player_ability", "player_ult", "player_damage", "player_death"]


def run_one(seconds: int, map_name: str, character: str | None, items: bool = False,
            ui: bool = False) -> tuple[int, dict | None]:
    """Runs one headless match; prints the detailed result and returns (exit code, report)."""
    if REPORT.exists():
        REPORT.unlink()  # never report a stale result

    cmd = [str(ENGINE_CMD), str(UPROJECT), map_name, "-game", "-nullrhi", "-unattended", "-nosound",
           "-nosplash", "-NoLiveCoding", f"-SkySmoke={seconds}", "-log=smoke.log"]
    if character:
        cmd.append(f"-SkyCharacter={character}")
    if items:
        cmd.append("-SkyItemTest")
    if ui:
        cmd.append("-SkyUISmoke")
    limit = seconds + STARTUP_GRACE_S
    who = f" as {character}" if character else ""
    print(f"[smoke] running {map_name}{who} headless for {seconds} s (timeout {limit} s)...", flush=True)
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

    report = None
    if REPORT.exists():
        try:
            r = json.loads(REPORT.read_text(encoding="utf-8"))
            report = r
            print(f"character: {r.get('character')} (id {r.get('character_id')})")
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
            item_test = r.get("item_test")
            if item_test is not None:
                print_item_test(item_test)
            elif items:
                print("item_test: missing from the report")
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
            # The UI run's whole point is its per-widget lines; a match shows the tail
            for l in (lines if ui else lines[-5:]):
                print("  " + l)

    if code == 0 and report is None:
        return 1, None  # exited cleanly without running the test: treat as failure
    if code == 0 and character and report.get("character_id") != character:
        print(f"[smoke] asked for {character} but the run played {report.get('character_id')}")
        return 1, report
    return code, report


def print_item_test(t: dict) -> None:
    fails = t.get("failures") or []
    print(f"item_test: ran={t.get('ran')} items_checked={t.get('items_checked')}/{t.get('catalog_items')} "
          f"checks={t.get('checks')} failures={t.get('fail_count', len(fails))}")
    cdr = t.get("cdr_stack")
    if cdr:
        print(f"  cdr_stack: items={cdr.get('items')} sum={cdr.get('sum')} "
              f"multiplier={cdr.get('multiplier')} ult_multiplier={cdr.get('ult_multiplier')}")
    pen = t.get("pen_stack")
    if pen:
        print(f"  pen_stack: items={pen.get('items')} penetration={pen.get('penetration')}")
    for m in t.get("pen_math") or []:
        print(f"  pen_math: pen={m.get('pen')} vs {m.get('god')} prot={m.get('god_prot')} "
              f"expected_loss={m.get('expected_loss')} loss={m.get('loss')}")
    for f in fails:
        print(f"  FAIL {f}")


def summary_line(character: str, code: int, report: dict | None) -> str:
    status = "PASS" if code == 0 else f"FAIL(exit={code})"
    if report is None:
        return f"{character:<13} {status}  no report"
    c = report.get("counters") or {}
    counts = " ".join(f"{k}={c.get(k, 0)}" for k in SUMMARY_COUNTERS)
    fails = report.get("failures") or []
    tail = f"  failures: {'; '.join(fails)}" if fails else ""
    return f"{character:<13} {status}  played={report.get('character')}  {counts}{tail}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seconds", type=int, default=120, help="match length to simulate (default 120)")
    ap.add_argument("--map", default="Joust", help="map to load (default Joust)")
    group = ap.add_mutually_exclusive_group()
    group.add_argument("--character", help="character-select ID to play (TriggerHappy, Hex, TreeRex, Spyro)")
    group.add_argument("--all-characters", action="store_true", help="run every playable kit back to back")
    ap.add_argument("--items", action="store_true", help="also run the item stat check (-SkyItemTest)")
    ap.add_argument("--ui", action="store_true",
                    help="UI construction check on the MainMenu map (-SkySmoke=10 -SkyUISmoke)")
    args = ap.parse_args()

    if args.ui:
        code, _ = run_one(10, "MainMenu", None, ui=True)
        return code

    if not args.all_characters:
        code, _ = run_one(args.seconds, args.map, args.character, args.items)
        return code

    results = []
    for ch in ALL_CHARACTERS:
        print(f"\n========== {ch} ==========", flush=True)
        code, report = run_one(args.seconds, args.map, ch, args.items)
        results.append((ch, code, report))
    print("\n========== summary ==========")
    for ch, code, report in results:
        print(summary_line(ch, code, report))
    return 0 if all(code == 0 for _, code, _ in results) else 1


if __name__ == "__main__":
    sys.exit(main())
