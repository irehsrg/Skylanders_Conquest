# Imports every WAV in <Project>/Saved/SFXStaging into /Game/Audio/SFX as USoundWave.
# Generate the WAVs first with:  python Scripts/gen_placeholder_sfx.py
# Run headless:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
# Idempotent: replace_existing=True re-imports over existing assets of the same name.
import os
import unreal

DEST = "/Game/Audio/SFX"
STAGING = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "Saved", "SFXStaging"))

# Sounds that belong to the UI sound group; everything else is Effects.
UI_SOUNDS = {"ui_click", "buy_item"}


def _sound_group(name):
    enum = getattr(unreal, "SoundGroup", None)
    if enum is None:
        return None
    attr = "SOUNDGROUP_UI" if name in UI_SOUNDS else "SOUNDGROUP_EFFECTS"
    return getattr(enum, attr, None)


wavs = sorted(f for f in os.listdir(STAGING) if f.lower().endswith(".wav")) if os.path.isdir(STAGING) else []
if not wavs:
    unreal.log_warning("No WAVs found in %s" % STAGING)

tasks = []
for f in wavs:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(STAGING, f))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", os.path.splitext(f)[0])
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

count = 0
for t in tasks:
    name = t.get_editor_property("destination_name")
    asset_path = DEST + "/" + name
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        unreal.log_error("FAILED import: %s" % t.get_editor_property("filename"))
        continue
    if isinstance(asset, unreal.SoundWave):
        grp = _sound_group(name)
        if grp is not None:
            try:
                asset.set_editor_property("sound_group", grp)
                unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            except Exception as e:  # property may not be editor-exposed in this version
                unreal.log_warning("sound_group not set on %s: %s" % (name, e))
    print(asset.get_path_name())
    count += 1

unreal.EditorAssetLibrary.save_directory(DEST)
print("IMPORTED %d" % count)
