# Imports every Spyro animation from the asset rip onto the already-imported
# Spyro skeleton, so he can be a playable god (the AI import only pulled the
# locomotion set). Idempotent: re-imports replace existing assets.
#
# Run headless with the editor CLOSED:
#   .\Scripts\RunEditorPython.ps1 -Script Scripts\ImportSpyroPlayableAnims.py
#
# Override the character with  set SKY_CHAR=Cynder  (folder names must match
# both the rip and /Game/Characters/<Name>/Models/<Name>_Skeleton).

import os
import unreal

CHAR = os.environ.get("SKY_CHAR", "Spyro").strip()
PROJECT_DIR = unreal.Paths.project_dir()
RIP = os.path.join(PROJECT_DIR, "Content", "Skylanders-Models")
ANIM_DIR = os.path.join(RIP, "Assets", "Skylanders", "Animations", CHAR).replace("\\", "/")
DEST = "/Game/Characters/%s/Animations" % CHAR
SKELETON_PATH = "/Game/Characters/%s/Models/%s_Skeleton" % (CHAR, CHAR)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

skeleton = unreal.EditorAssetLibrary.load_asset(SKELETON_PATH)
if not skeleton:
    unreal.log_error("No skeleton at " + SKELETON_PATH)
    raise SystemExit(1)
if not os.path.isdir(ANIM_DIR):
    unreal.log_error("No anim dir: " + ANIM_DIR)
    raise SystemExit(1)

files = sorted(f for f in os.listdir(ANIM_DIR) if f.lower().endswith(".fbx"))
unreal.log("Importing %d %s animations from %s" % (len(files), CHAR, ANIM_DIR))

ok, failed = [], []
for fbx in files:
    full = os.path.join(ANIM_DIR, fbx).replace("\\", "/")
    task = unreal.AssetImportTask()
    task.set_editor_property("automated", True)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("filename", full)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("skeleton", skeleton)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    anim_options = options.get_editor_property("anim_sequence_import_data")
    if anim_options:
        anim_options.set_editor_property("import_bone_tracks", True)
        anim_options.set_editor_property("remove_redundant_keys", False)
    task.set_editor_property("options", options)

    asset_tools.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    if paths:
        ok.append(os.path.splitext(fbx)[0])
    else:
        failed.append(fbx)

unreal.EditorAssetLibrary.save_directory(DEST)
unreal.log("ANIMS OK %d: %s" % (len(ok), " ".join(ok)))
if failed:
    unreal.log_warning("ANIMS FAILED %d: %s" % (len(failed), " ".join(failed)))
print("IMPORTED %d" % len(ok))
