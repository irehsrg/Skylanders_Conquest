# Fix drive_run animation rotation - re-imports with -90 yaw offset
# Run in Output Log (Python mode):
#   "C:/Users/ajoin/Unreal Engine/Skylanders_Conquest/Scripts/FixRunAnimation.py"

import unreal

source = "C:/Users/ajoin/Downloads/Skylanders-Models-main/Skylanders-Models-main/Assets/Skylanders/Animations/TriggerHappy/drive_run.fbx"
target = "/Game/Characters/TriggerHappy/Animations"
skeleton_path = "/Game/Characters/TriggerHappy/Models/TriggerHappy_Skeleton.TriggerHappy_Skeleton"

skeleton = unreal.load_asset(skeleton_path)
if not skeleton:
    skeleton = unreal.load_asset("/Game/Characters/TriggerHappy/Models/TriggerHappy_Skeleton")

unreal.log("Skeleton: " + str(skeleton))

task = unreal.AssetImportTask()
task.set_editor_property('automated', True)
task.set_editor_property('destination_path', target)
task.set_editor_property('filename', source)
task.set_editor_property('replace_existing', True)
task.set_editor_property('save', True)

options = unreal.FbxImportUI()
options.set_editor_property('import_mesh', False)
options.set_editor_property('import_animations', True)
options.set_editor_property('import_materials', False)
options.set_editor_property('import_textures', False)
options.set_editor_property('skeleton', skeleton)
options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)

# Apply -90 yaw rotation to fix the run animation facing right
anim_data = options.get_editor_property('anim_sequence_import_data')
if anim_data:
    anim_data.set_editor_property('import_rotation', unreal.Rotator(0, -90, 0))
    unreal.log("Set import rotation to (0, -90, 0)")

task.set_editor_property('options', options)

try:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log("drive_run re-imported with -90 yaw rotation fix!")
except RuntimeError as e:
    if "BehaviorHack" in str(e) or "Unable to retrieve bone index" in str(e):
        unreal.log("drive_run re-imported with rotation fix (bone warning ignored)")
    else:
        unreal.log_error("Failed: " + str(e))
