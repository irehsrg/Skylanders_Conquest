# Imports the real Skylander models used by the AI gods: skeletal mesh,
# a filtered set of animations, staged textures, and a body material.
#
# Modeled on ImportHexAndTreeRex.py. Run headless with the editor CLOSED:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript
#       -script="Scripts/ImportGodCharacters.py" -unattended -nullrhi

import os
import re

import unreal

REPO = "C:/Users/ajoin/Downloads/Skylanders-Models"
STAGING = REPO + "/Staging"

# Override with a comma-separated list, e.g.  set SKY_CHARS=Terrabite,Drobit
CHARACTERS = [c for c in os.environ.get(
    'SKY_CHARS', "Spyro,StealthElf,KaosSensei,ChompyMage,Cynder").split(',') if c.strip()]

# Only import animations we might actually use — the full rip is ~50-60 per
# character and most are cutscene/kart/knockback one-offs.
ANIM_PATTERN = re.compile(
    r"(drive_idle|drive_run|drive_modswap_pose01|emotion_idle|flight_idle|"
    r"flight_slow|charge_fast|charge_in|run_stop|land_running|magicmoment_)",
    re.IGNORECASE)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_skeletal_mesh(fbx_path, dest_path):
    task = unreal.AssetImportTask()
    task.set_editor_property('automated', True)
    task.set_editor_property('destination_path', dest_path)
    task.set_editor_property('filename', fbx_path)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)

    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_as_skeletal', True)
    options.set_editor_property('import_animations', False)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('create_physics_asset', True)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)

    task.set_editor_property('options', options)
    asset_tools.import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths'))


def import_animations(anim_dir, dest_path, skeleton):
    if not os.path.isdir(anim_dir):
        unreal.log_warning('  no anim dir: ' + anim_dir)
        return 0
    files = sorted(f for f in os.listdir(anim_dir)
                   if f.lower().endswith('.fbx') and ANIM_PATTERN.search(f))
    ok = 0
    for fbx_file in files:
        full_path = os.path.join(anim_dir, fbx_file).replace("\\", "/")
        task = unreal.AssetImportTask()
        task.set_editor_property('automated', True)
        task.set_editor_property('destination_path', dest_path)
        task.set_editor_property('filename', full_path)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)

        options = unreal.FbxImportUI()
        options.set_editor_property('import_mesh', False)
        options.set_editor_property('import_animations', True)
        options.set_editor_property('import_materials', False)
        options.set_editor_property('import_textures', False)
        options.set_editor_property('import_as_skeletal', False)
        options.set_editor_property('skeleton', skeleton)
        options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
        anim_options = options.get_editor_property('anim_sequence_import_data')
        if anim_options:
            anim_options.set_editor_property('import_bone_tracks', True)
            anim_options.set_editor_property('remove_redundant_keys', False)

        task.set_editor_property('options', options)
        try:
            asset_tools.import_asset_tasks([task])
            ok += 1
        except RuntimeError as e:
            msg = str(e)
            # These rips throw benign bone-remap warnings; count them as fine.
            if "BehaviorHack" in msg or "Unable to retrieve bone index" in msg:
                ok += 1
            else:
                unreal.log_warning("  ANIM FAILED: {} - {}".format(fbx_file, msg))
    return ok


def import_textures(texture_dir, dest_path):
    imported = {}
    if not os.path.isdir(texture_dir):
        return imported
    for png in sorted(f for f in os.listdir(texture_dir) if f.lower().endswith('.png')):
        task = unreal.AssetImportTask()
        task.set_editor_property('automated', True)
        task.set_editor_property('destination_path', dest_path)
        task.set_editor_property('filename', os.path.join(texture_dir, png).replace("\\", "/"))
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        asset_tools.import_asset_tasks([task])
        paths = list(task.get_editor_property('imported_object_paths'))
        if paths:
            name = os.path.splitext(png)[0]
            imported[name] = paths[0]
            tex = unreal.load_asset(paths[0])
            if tex and name.endswith('_N'):
                tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property('srgb', False)
                unreal.EditorAssetLibrary.save_asset(paths[0].split('.')[0])
    return imported


def make_material(mat_name, dest_path, color_path, normal_path):
    existing = unreal.load_asset(dest_path + "/" + mat_name)
    mat = existing or asset_tools.create_asset(mat_name, dest_path, unreal.Material,
                                               unreal.MaterialFactoryNew())
    if not mat:
        return None
    mel = unreal.MaterialEditingLibrary

    color_tex = unreal.load_asset(color_path) if color_path else None
    if color_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -200)
        node.set_editor_property('texture', color_tex)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    normal_tex = unreal.load_asset(normal_path) if normal_path else None
    if normal_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 300)
        node.set_editor_property('texture', normal_tex)
        node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_NORMAL)

    # Without this the material renders GRAY on skeletal meshes.
    mat.set_editor_property('used_with_skeletal_mesh', True)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(dest_path + "/" + mat_name)
    return mat


for name in CHARACTERS:
    base = "/Game/Characters/" + name
    unreal.log("=" * 60)
    unreal.log("=== Importing {} ===".format(name))

    fbx = "{}/Assets/Skylanders/Actors/{}.fbx".format(REPO, name)
    if not os.path.isfile(fbx):
        unreal.log_error("missing FBX: " + fbx)
        continue

    import_skeletal_mesh(fbx, base + "/Models")

    skeleton, mesh_path = None, None
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in registry.get_assets_by_path(base + "/Models", recursive=True):
        cls = str(a.asset_class_path.asset_name)
        if cls == "Skeleton":
            skeleton = a.get_asset()
        elif cls == "SkeletalMesh":
            mesh_path = str(a.package_name)
    if not skeleton:
        unreal.log_error("no skeleton for " + name)
        continue

    n = import_animations("{}/Assets/Skylanders/Animations/{}".format(REPO, name),
                          base + "/Animations", skeleton)
    unreal.log("  animations imported: {}".format(n))

    import_textures(STAGING + "/" + name, base + "/Textures")

    tex_base = base + "/Textures/"
    body_mat = make_material("M_" + name + "_Body", base + "/Materials",
                             tex_base + name + "_C", tex_base + name + "_N")

    if mesh_path and body_mat:
        mesh = unreal.load_asset(mesh_path)
        if mesh:
            mats = mesh.get_editor_property('materials')
            new_mats = []
            for m in mats:
                sm = unreal.SkeletalMaterial()
                sm.set_editor_property('material_interface', body_mat)
                sm.set_editor_property('material_slot_name', m.get_editor_property('material_slot_name'))
                new_mats.append(sm)
            mesh.set_editor_property('materials', new_mats)
            unreal.EditorAssetLibrary.save_asset(mesh_path)
            unreal.log("  mesh {} -> {} material slots".format(mesh_path, len(new_mats)))

unreal.log("=" * 60)
unreal.log("=== God character import complete ===")
