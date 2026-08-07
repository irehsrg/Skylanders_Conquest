# Imports the converted Skylanders level_505 static geometry (FBX) as Unreal
# static meshes, then writes a bounds report so the map builder can pick meshes
# by real-world size.
#
# The FBX come from docs/ASSET-EXTRACTION.md's pipeline, batch-converted with
# D:/SkylandersExtract/convert_batch.sh (see that script for the undocumented
# IgzModelConverterGUI CLI it drives).
#
# Materials/textures are NOT imported: the converter does not embed them, and
# the map already has authentic surface materials in /Game/Environment.
#
# Two things about these rips make a plain import fail:
#   * The UE5 Interchange FBX parser ASSERTS (hard crash, takes the whole
#     commandlet with it) on their UV layers, so we force the legacy importer.
#   * A few still crash or contain no importable mesh, so this is crash-safe:
#     it records the file it is about to touch in a marker, and on the next run
#     any file named by a leftover marker is blacklisted. Already-imported
#     assets are skipped, so re-running resumes where it died.
# Drive it with Scripts/import_level_geometry.sh, which loops until it settles.
#
# Headless, editor CLOSED:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript
#       -script="<abs>/Scripts/ImportLevelGeometry.py" -unattended -nullrhi
#
# Env:
#   SKY_GEO_DIR  source folder of .fbx  (default: the batch output dir)
#   SKY_GEO_DEST content path to import into

import json
import os

import unreal

SRC = os.environ.get('SKY_GEO_DIR', r"D:/SkylandersExtract/fbx_nx9_statics")
DEST = os.environ.get('SKY_GEO_DEST', '/Game/Environment/Level505')
# Existing assets are skipped so a crashed run can resume. Set SKY_GEO_FORCE=1
# to re-import over them -- needed after the 2026-08-07 extraction fix, because
# everything imported before it was built from corrupted FBX and would
# otherwise be silently kept.
#
# Force must still converge. "Skip what already exists" is ALSO the resume
# mechanism, so simply ignoring it makes every pass redo all 133 files, crash at
# the same place, and never finish -- 45 passes and 29 minutes to import 107 of
# 133. So a forced run records each asset it has re-imported and skips those
# instead, which forces exactly one re-import per asset and still resumes.
FORCE = os.environ.get('SKY_GEO_FORCE', '') == '1'
REPORT = os.environ.get('SKY_GEO_REPORT', SRC + '/_bounds.json')
BLACKLIST = SRC + '/_import_blacklist.txt'
REIMPORTED = SRC + '/_reimported.txt'
MARKER = SRC + '/_importing.marker'

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Route FBX through the legacy importer; Interchange asserts on these files.
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')


def read_lines(path):
    if not os.path.isfile(path):
        return []
    with open(path) as fh:
        return [ln.strip() for ln in fh if ln.strip()]


def append_line(path, value):
    with open(path, 'a') as fh:
        fh.write(value + '\n')


def claim_crashed_file(blacklist):
    """A leftover marker means last run died mid-import on that file."""
    stale = read_lines(MARKER)
    if stale:
        victim = stale[0]
        if victim not in blacklist:
            blacklist.append(victim)
            append_line(BLACKLIST, victim)
            unreal.log_warning('blacklisting crash-inducing FBX: ' + victim)
    if os.path.isfile(MARKER):
        os.remove(MARKER)
    return blacklist


def make_options():
    """Static-mesh-only FBX import: no materials, no skeleton, combined."""
    opts = unreal.FbxImportUI()
    opts.set_editor_property('import_mesh', True)
    opts.set_editor_property('import_as_skeletal', False)
    opts.set_editor_property('import_animations', False)
    opts.set_editor_property('import_materials', False)
    opts.set_editor_property('import_textures', False)
    opts.set_editor_property('mesh_type_to_import',
                             unreal.FBXImportType.FBXIT_STATIC_MESH)

    sm = opts.static_mesh_import_data
    # One asset per file: these rips are split into many small geometry nodes.
    sm.set_editor_property('combine_meshes', True)
    sm.set_editor_property('generate_lightmap_u_vs', True)
    sm.set_editor_property('auto_generate_collision', True)
    sm.set_editor_property('remove_degenerates', True)
    # The rip is Z-up/centimetre already; don't let the importer re-orient it.
    sm.set_editor_property('convert_scene', True)
    sm.set_editor_property('force_front_x_axis', False)
    return opts


def import_all():
    if not os.path.isdir(SRC):
        unreal.log_error('missing geometry dir: ' + SRC)
        return

    blacklist = claim_crashed_file(read_lines(BLACKLIST))
    reimported = set(read_lines(REIMPORTED)) if FORCE else set()
    files = sorted(f for f in os.listdir(SRC) if f.lower().endswith('.fbx'))
    opts = make_options()

    done = skipped = failed = 0
    for fbx in files:
        name = os.path.splitext(fbx)[0]
        if fbx in blacklist:
            skipped += 1
            continue
        if FORCE:
            if fbx in reimported:        # already replaced on an earlier pass
                done += 1
                continue
        elif unreal.EditorAssetLibrary.does_asset_exist(DEST + '/' + name):
            done += 1
            continue

        # If the import hard-crashes, this marker tells the next run who did it.
        with open(MARKER, 'w') as fh:
            fh.write(fbx + '\n')

        task = unreal.AssetImportTask()
        task.set_editor_property('automated', True)
        task.set_editor_property('destination_path', DEST)
        task.set_editor_property('filename', os.path.join(SRC, fbx).replace('\\', '/'))
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        task.set_editor_property('options', opts)
        try:
            asset_tools.import_asset_tasks([task])
        except Exception as exc:                      # noqa: BLE001 - report and continue
            unreal.log_warning('import raised on {}: {}'.format(fbx, exc))

        if task.get_editor_property('imported_object_paths'):
            done += 1
            if FORCE:
                append_line(REIMPORTED, fbx)
        else:
            failed += 1
            unreal.log_warning('no mesh imported from ' + fbx)

        os.remove(MARKER)

    unreal.log('import pass: {} present, {} blacklisted, {} empty'.format(
        done, skipped, failed))


def report():
    """Dump per-mesh extent/tri-count so map code can choose meshes by size."""
    rows = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for asset in registry.get_assets_by_path(DEST, recursive=True):
        if str(asset.asset_class_path.asset_name) != 'StaticMesh':
            continue
        mesh = asset.get_asset()
        if not mesh:
            continue
        bounds = mesh.get_bounds()          # box_extent is half-size, in cm
        ext, origin = bounds.box_extent, bounds.origin
        rows.append({
            'name': str(asset.asset_name),
            'path': str(asset.package_name),
            'extent': [round(ext.x, 1), round(ext.y, 1), round(ext.z, 1)],
            'origin': [round(origin.x, 1), round(origin.y, 1), round(origin.z, 1)],
            'tris': mesh.get_num_triangles(0),
        })

    rows.sort(key=lambda r: -max(r['extent']))
    try:
        with open(REPORT, 'w') as fh:
            json.dump(rows, fh, indent=1)
    except IOError as exc:
        unreal.log_error('could not write report: {}'.format(exc))
    unreal.log('=== ImportLevelGeometry: {} static meshes in {} ==='.format(
        len(rows), DEST))


import_all()
report()
