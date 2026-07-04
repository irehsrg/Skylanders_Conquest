# Unreal Editor Python Script - Import Hex and Tree Rex (meshes, animations, textures, materials)
#
# Modeled on ImportTriggerHappyAnimations.py. Run from the editor Python console:
#   py "C:/Users/ajoin/Unreal Engine/Skylanders_Conquest/Scripts/ImportHexAndTreeRex.py"

import unreal
import os

REPO = "C:/Users/ajoin/Downloads/Skylanders-Models"
STAGING = REPO + "/Staging"

CHARACTERS = [
    {
        "name": "Hex",
        "actor_fbx": REPO + "/Assets/Skylanders/Actors/Hex.fbx",
        "anim_dir": REPO + "/Assets/Skylanders/Animations/Hex",
        "texture_dir": STAGING + "/Hex",
    },
    {
        "name": "TreeRex",
        "actor_fbx": REPO + "/Assets/Skylanders/Actors/TreeRex.fbx",
        "anim_dir": REPO + "/Assets/Skylanders/Animations/TreeRex",
        "texture_dir": STAGING + "/TreeRex",
    },
]

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
    paths = list(task.get_editor_property('imported_object_paths'))
    unreal.log("Mesh import -> {}".format(paths))
    return paths


def import_animations(anim_dir, dest_path, skeleton):
    fbx_files = sorted(f for f in os.listdir(anim_dir) if f.lower().endswith('.fbx'))
    ok, fail = 0, 0
    for fbx_file in fbx_files:
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
            if "BehaviorHack" in msg or "Unable to retrieve bone index" in msg:
                ok += 1
            else:
                fail += 1
                unreal.log_warning("  ANIM FAILED: {} - {}".format(fbx_file, msg))
    unreal.log("Animations: {} ok, {} failed".format(ok, fail))


def import_textures(texture_dir, dest_path):
    imported = {}
    for png in sorted(os.listdir(texture_dir)):
        if not png.lower().endswith('.png'):
            continue
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
                # Make sure normal maps import as normal maps
                tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property('srgb', False)
                unreal.EditorAssetLibrary.save_asset(paths[0].split('.')[0])
    unreal.log("Textures imported: {}".format(sorted(imported.keys())))
    return imported


def make_material(mat_name, dest_path, color_path, emissive_path, normal_path):
    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset(mat_name, dest_path, unreal.Material, factory)
    if not mat:
        # Already exists - reuse
        mat = unreal.load_asset(dest_path + "/" + mat_name)
    mel = unreal.MaterialEditingLibrary

    color_tex = unreal.load_asset(color_path) if color_path else None
    if color_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -200)
        node.set_editor_property('texture', color_tex)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    emissive_tex = unreal.load_asset(emissive_path) if emissive_path else None
    if emissive_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 100)
        node.set_editor_property('texture', emissive_tex)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    normal_tex = unreal.load_asset(normal_path) if normal_path else None
    if normal_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 400)
        node.set_editor_property('texture', normal_tex)
        node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(dest_path + "/" + mat_name)
    unreal.log("Material created: {}/{}".format(dest_path, mat_name))
    return mat


for char in CHARACTERS:
    name = char["name"]
    base = "/Game/Characters/" + name
    unreal.log("=" * 50)
    unreal.log("=== Importing " + name + " ===")

    # 1. Skeletal mesh (creates skeleton + physics asset)
    import_skeletal_mesh(char["actor_fbx"], base + "/Models")

    # Find the skeleton that was just created
    skeleton = None
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(base + "/Models", recursive=True)
    mesh_path = None
    for a in assets:
        cls = str(a.asset_class_path.asset_name)
        if cls == "Skeleton":
            skeleton = a.get_asset()
        elif cls == "SkeletalMesh":
            mesh_path = str(a.package_name)
    if not skeleton:
        unreal.log_error("No skeleton found for " + name + ", skipping animations")
        continue
    unreal.log("Skeleton: {}  Mesh: {}".format(skeleton.get_path_name(), mesh_path))

    # 2. Animations
    import_animations(char["anim_dir"], base + "/Animations", skeleton)

    # 3. Textures
    tex = import_textures(char["texture_dir"], base + "/Textures")

    # 4. Body material (color + emissive + normal)
    tex_base = base + "/Textures/"
    body_c = tex_base + ("Hex_C" if name == "Hex" else "TreeRex_C")
    body_e = tex_base + ("Hex_E" if name == "Hex" else "TreeRex_E")
    body_n = tex_base + ("Hex_N" if name == "Hex" else "TreeRex_N")
    body_mat = make_material("M_" + name + "_Body", base + "/Materials", body_c, body_e, body_n)

    # 5. Assign the body material to every slot on the mesh (refined later if slots differ)
    if mesh_path:
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
            unreal.log("Assigned M_{}_Body to {} slots; slot names: {}".format(
                name, len(new_mats), [str(m.get_editor_property('material_slot_name')) for m in mats]))

unreal.log("=" * 50)
unreal.log("=== Hex + TreeRex import complete ===")
