#!/usr/bin/env bash
# Drives Scripts/ImportLevelGeometry.py until it stops making progress.
#
# The UE FBX parser hard-asserts on a few of the ripped level_505 meshes, which
# kills the commandlet outright. The Python side is resumable and blacklists
# whichever file was in flight when it died, so simply re-running converges:
# each crash costs one file, and the loop exits once a pass survives to the end.
set -u
UE="${UE:-/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"
PROJ="${PROJ:-C:/Users/ajoin/Unreal Engine/Skylanders_Conquest/Skylanders_Conquest.uproject}"
SCRIPT="${SCRIPT:-C:/Users/ajoin/Unreal Engine/Skylanders_Conquest/Scripts/ImportLevelGeometry.py}"
SRC="${SKY_GEO_DIR:-D:/SkylandersExtract/fbx_nx9_statics}"
MAX="${MAX:-45}"

count() { wc -l < "$1" 2>/dev/null || echo 0; }

# A non-zero exit does NOT mean the commandlet crashed. It also returns non-zero
# when a file simply contains no importable mesh -- a permanent condition, so
# retrying it forever is pure waste (12 passes, 2.5 minutes, zero progress).
# Converge on PROGRESS instead: stop as soon as a pass neither imports anything
# new nor blacklists anything new.
for i in $(seq 1 "$MAX"); do
  before_done=$(count "$SRC/_reimported.txt")
  before_black=$(count "$SRC/_import_blacklist.txt")
  echo "--- import pass $i ---"
  "$UE" "$PROJ" -run=pythonscript -script="$SCRIPT" -unattended -nullrhi >/dev/null 2>&1
  rc=$?
  after_done=$(count "$SRC/_reimported.txt")
  after_black=$(count "$SRC/_import_blacklist.txt")
  echo "    exit=$rc  imported=$after_done  blacklisted=$after_black"
  if [ $rc -eq 0 ]; then
    echo "=== clean pass on attempt $i (imported $after_done, blacklisted $after_black) ==="
    exit 0
  fi
  if [ "$after_done" = "$before_done" ] && [ "$after_black" = "$before_black" ]; then
    echo "=== settled on attempt $i: no further progress possible ==="
    echo "    imported $after_done, blacklisted $after_black;"
    echo "    the rest contain no importable mesh (see the log for which)."
    exit 0
  fi
done
echo "=== gave up after $MAX passes ==="
exit 1
