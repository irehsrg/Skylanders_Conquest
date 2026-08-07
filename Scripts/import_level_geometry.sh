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
SRC="${SKY_GEO_DIR:-D:/SkylandersExtract/fbx/level_505_statics}"
MAX="${MAX:-45}"

for i in $(seq 1 "$MAX"); do
  echo "--- import pass $i ---"
  "$UE" "$PROJ" -run=pythonscript -script="$SCRIPT" -unattended -nullrhi >/dev/null 2>&1
  rc=$?
  blacklisted=$(wc -l < "$SRC/_import_blacklist.txt" 2>/dev/null || echo 0)
  echo "    exit=$rc  blacklisted=$blacklisted"
  if [ $rc -eq 0 ]; then
    echo "=== clean pass on attempt $i (blacklisted $blacklisted file(s)) ==="
    exit 0
  fi
done
echo "=== gave up after $MAX passes ==="
exit 1
