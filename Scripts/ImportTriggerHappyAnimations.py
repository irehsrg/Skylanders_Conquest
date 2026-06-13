# Unreal Editor Python Script - Import TriggerHappy Animations
#
# HOW TO RUN:
# 1. Open Unreal Editor
# 2. Enable the "Python Editor Script Plugin" if not already enabled
#    (Edit > Plugins > search "Python" > enable "Python Editor Script Plugin" > restart editor)
# 3. Open the Output Log (Window > Output Log)
# 4. At the bottom of the Output Log, change the dropdown from "Cmd" to "Python"
# 5. If dropdown says "Python", just paste the path (no py prefix):
#    "C:/Users/ajoin/Unreal Engine/Skylanders_Conquest/Scripts/ImportTriggerHappyAnimations.py"
#    If dropdown says "Cmd", use the py prefix:
#    py "C:/Users/ajoin/Unreal Engine/Skylanders_Conquest/Scripts/ImportTriggerHappyAnimations.py"
# 6. Press Enter and wait for it to finish

import unreal
import os

# Source directory with FBX files
source_dir = "C:/Users/ajoin/Downloads/Skylanders-Models-main/Skylanders-Models-main/Assets/Skylanders/Animations/TriggerHappy"

# Destination in Content Browser
target_dir = "/Game/Characters/TriggerHappy/Animations"

# Skeleton to target
skeleton_path = "/Game/Characters/TriggerHappy/Models/TriggerHappy_Skeleton.TriggerHappy_Skeleton"

# Load skeleton
skeleton = unreal.load_asset(skeleton_path)
if not skeleton:
    unreal.log_error("Could not load skeleton at: " + skeleton_path)
    # Try alternate path without double name
    skeleton_path2 = "/Game/Characters/TriggerHappy/Models/TriggerHappy_Skeleton"
    skeleton = unreal.load_asset(skeleton_path2)
    if not skeleton:
        unreal.log_error("Could not load skeleton at alternate path either. Aborting.")
        raise RuntimeError("Skeleton not found")

unreal.log("Loaded skeleton: " + str(skeleton))

# Get all FBX files
fbx_files = [f for f in os.listdir(source_dir) if f.lower().endswith('.fbx')]
fbx_files.sort()

unreal.log("Found {} FBX files to import".format(len(fbx_files)))

# Import each FBX as animation
tasks = []
for fbx_file in fbx_files:
    full_path = os.path.join(source_dir, fbx_file).replace("\\", "/")

    task = unreal.AssetImportTask()
    task.set_editor_property('automated', True)
    task.set_editor_property('destination_path', target_dir)
    task.set_editor_property('filename', full_path)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)

    # FBX import options - animation only
    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', False)
    options.set_editor_property('import_animations', True)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('skeleton', skeleton)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)

    # Animation-specific settings
    anim_options = options.get_editor_property('anim_sequence_import_data')
    if anim_options:
        anim_options.set_editor_property('import_bone_tracks', True)
        anim_options.set_editor_property('remove_redundant_keys', False)

    task.set_editor_property('options', options)
    tasks.append(task)

# Run all imports
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

success_count = 0
fail_count = 0

for i, task in enumerate(tasks):
    fbx_name = fbx_files[i]
    unreal.log("Importing [{}/{}]: {}".format(i + 1, len(tasks), fbx_name))

    try:
        asset_tools.import_asset_tasks([task])
        success_count += 1
        unreal.log("  OK: " + fbx_name)
    except RuntimeError as e:
        # "BehaviorHack" bone track warning - animation still imports fine, just has extra tracks
        err_msg = str(e)
        if "BehaviorHack" in err_msg or "Unable to retrieve bone index" in err_msg:
            success_count += 1
            unreal.log("  OK (with bone track warning): " + fbx_name)
        else:
            fail_count += 1
            unreal.log_warning("  FAILED: {} - {}".format(fbx_name, err_msg))

unreal.log("=" * 50)
unreal.log("Import complete! Success: {}, Failed: {}".format(success_count, fail_count))
unreal.log("Animations should be in: " + target_dir)
unreal.log("=" * 50)
