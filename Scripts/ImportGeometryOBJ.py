# Imports static geometry from OBJ, as a fallback for the meshes the FBX route
# cannot deliver.
#
# After the 2026-08-07 extraction fix, 125 of 133 level_505 statics import from
# the converter's FBX. The remaining 8 fail for two reasons, neither of which is
# a data problem any more:
#   * l505_nautical_area, l505_water     - hard-crash the UE FBX parser
#   * clouds_*, L505_lanturn, and three  - the converter emits an FBX with no
#     animated props                       importable mesh at all
# plus l505_steinhouse, whose asset is still the pre-fix corrupt import because
# its FBX crashes the parser too.
#
# Scripts/igz_mesh.py reads all nine correctly (l505_nautical_area is 46589
# verts / 58890 tris), so this imports its OBJ output instead. That is the point
# of having a reader that does not depend on the converter.
#
# Headless, editor CLOSED:
#   SKY_OBJ_DIR=<dir of .obj> UnrealEditor-Cmd.exe <uproject> -run=pythonscript
#       -script="<abs>/Scripts/ImportGeometryOBJ.py" -unattended -nullrhi
#
# Env:
#   SKY_OBJ_DIR   source folder of .obj
#   SKY_OBJ_DEST  content path to import into
#   SKY_OBJ_ONLY  optional comma-separated list of basenames to restrict to

import os

import unreal

SRC = os.environ.get('SKY_OBJ_DIR', r"D:/SkylandersExtract/obj_nx9_all")
DEST = os.environ.get('SKY_OBJ_DEST', '/Game/Environment/Level505')
ONLY = [s.strip() for s in os.environ.get('SKY_OBJ_ONLY', '').split(',') if s.strip()]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def main():
    if not os.path.isdir(SRC):
        unreal.log_error('missing obj dir: ' + SRC)
        return

    names = ONLY or [os.path.splitext(f)[0]
                     for f in sorted(os.listdir(SRC)) if f.lower().endswith('.obj')]

    done = failed = 0
    for name in names:
        path = os.path.join(SRC, name + '.obj').replace('\\', '/')
        if not os.path.isfile(path):
            unreal.log_warning('no such obj: ' + path)
            failed += 1
            continue

        task = unreal.AssetImportTask()
        task.set_editor_property('automated', True)
        task.set_editor_property('destination_path', DEST)
        task.set_editor_property('destination_name', name)
        task.set_editor_property('filename', path)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        try:
            asset_tools.import_asset_tasks([task])
        except Exception as exc:                      # noqa: BLE001
            unreal.log_warning('import raised on {}: {}'.format(name, exc))

        if task.get_editor_property('imported_object_paths'):
            done += 1
            unreal.log('imported ' + name)
        else:
            failed += 1
            unreal.log_warning('no mesh imported from ' + name + '.obj')

    unreal.log('OBJ import: {} imported, {} failed'.format(done, failed))


main()
