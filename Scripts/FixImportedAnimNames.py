# Cleans up after a legacy-FBX animation import: some rip clips carry mesh data,
# so the importer also writes "<clip>" (a SkeletalMesh), "<clip>_PhysicsAsset"
# and puts the animation itself in "<clip>_Anim". This deletes the stray
# mesh/physics/skeleton assets and renames "<clip>_Anim" to "<clip>" so every
# animation has the name its FBX had.
#
#   .\Scripts\RunEditorPython.ps1 -Script Scripts\FixImportedAnimNames.py
#   set SKY_CHAR=Cynder  to target another character's Animations folder.

import os
import unreal

CHAR = os.environ.get("SKY_CHAR", "Spyro").strip()
FOLDER = "/Game/Characters/%s/Animations" % CHAR

eal = unreal.EditorAssetLibrary
paths = eal.list_assets(FOLDER, recursive=False, include_folder=False)
deleted, renamed, kept = [], [], 0

# Pass 1: delete everything that is not an animation sequence.
for p in paths:
    data = eal.find_asset_data(p)
    cls = data.asset_class_path.asset_name if data else None
    if cls in ("SkeletalMesh", "PhysicsAsset", "Skeleton"):
        if eal.delete_asset(p):
            deleted.append(p.split("/")[-1].split(".")[0])

# Pass 2: rename "<clip>_Anim" -> "<clip>".
for p in eal.list_assets(FOLDER, recursive=False, include_folder=False):
    name = p.split("/")[-1].split(".")[0]
    if not name.endswith("_Anim"):
        kept += 1
        continue
    base = name[:-5]
    target = "%s/%s" % (FOLDER, base)
    if eal.does_asset_exist(target):
        unreal.log_warning("target exists, deleting it first: " + target)
        eal.delete_asset(target)
    if eal.rename_asset(p, target):
        renamed.append(base)
    else:
        unreal.log_error("rename failed: " + p)

eal.save_directory(FOLDER)
unreal.log("DELETED %d: %s" % (len(deleted), " ".join(deleted)))
unreal.log("RENAMED %d: %s" % (len(renamed), " ".join(renamed)))
final = [q.split("/")[-1].split(".")[0] for q in eal.list_assets(FOLDER, recursive=False, include_folder=False)]
unreal.log("FINAL %d: %s" % (len(final), " ".join(sorted(final))))
print("DONE")
