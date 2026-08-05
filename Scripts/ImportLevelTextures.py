# Imports authentic Skylanders level surface textures and rebuilds the map's
# ground/terrain materials to use them instead of procedural noise.
#
# Rebuilds /Game/Environment/M_Ground and M_Terrain in place, so the map builder
# needs no code change (it already points at those paths).
#
# UVs are driven from WORLD POSITION rather than mesh UVs, because the map is
# built from scaled engine cubes whose UVs don't tile sanely at map scale.
#
# Headless, editor CLOSED:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript
#       -script="Scripts/ImportLevelTextures.py" -unattended -nullrhi

import os
import unreal

SRC = os.environ.get('SKY_LEVELTEX_DIR', r"C:/Users/ajoin/AppData/Local/Temp/claude/"
                     r"C--Users-ajoin-Unreal-Engine-Skylanders-Conquest/"
                     r"8a13b565-bcc0-4f34-a523-2111fb0f8fcd/scratchpad/LevelTex")
TEX_DEST = '/Game/Environment/Textures'
MAT_DEST = '/Game/Environment'

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def import_textures():
    if not os.path.isdir(SRC):
        unreal.log_error('missing texture dir: ' + SRC)
        return {}
    tasks = []
    for png in sorted(f for f in os.listdir(SRC) if f.lower().endswith('.png')):
        t = unreal.AssetImportTask()
        t.set_editor_property('automated', True)
        t.set_editor_property('destination_path', TEX_DEST)
        t.set_editor_property('filename', os.path.join(SRC, png).replace('\\', '/'))
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('save', True)
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)
    out = {}
    for t in tasks:
        for p in t.get_editor_property('imported_object_paths'):
            name = str(p).split('.')[-1]
            out[name] = str(p)
    unreal.log('imported textures: {}'.format(sorted(out.keys())))
    return out


def build_material(mat_name, texture, tile_size, tint=None):
    """Unlit material sampling `texture` in world space every `tile_size` units."""
    pkg = MAT_DEST + '/' + mat_name
    existing = unreal.load_asset(pkg)
    if existing:
        unreal.EditorAssetLibrary.delete_asset(pkg)  # rebuild clean
    mat = asset_tools.create_asset(mat_name, MAT_DEST, unreal.Material,
                                   unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error('could not create ' + mat_name)
        return None

    # World position -> XY -> scaled = tiling UVs independent of mesh UVs
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 0)
    mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -900, 0)
    mask.set_editor_property('r', True)
    mask.set_editor_property('g', True)
    mask.set_editor_property('b', False)
    mask.set_editor_property('a', False)
    mel.connect_material_expressions(wp, '', mask, '')

    scale = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -900, 150)
    scale.set_editor_property('r', 1.0 / float(tile_size))

    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -700, 0)
    mel.connect_material_expressions(mask, '', mul, 'A')
    mel.connect_material_expressions(scale, '', mul, 'B')

    samp = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 0)
    samp.set_editor_property('texture', texture)
    mel.connect_material_expressions(mul, '', samp, 'UVs')

    out_node = samp
    if tint:
        tint_node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 300)
        tint_node.set_editor_property('constant', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        tmul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 0)
        mel.connect_material_expressions(samp, 'RGB', tmul, 'A')
        mel.connect_material_expressions(tint_node, '', tmul, 'B')
        out_node = tmul

    # Unlit so the art's own colour survives (lit washes these stylised maps out)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(out_node, '' if tint else 'RGB',
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(pkg)
    unreal.log('built {} (tile {}u)'.format(mat_name, tile_size))
    return mat


tex = import_textures()

sand = unreal.load_asset(tex.get('Sand', TEX_DEST + '/Sand'))
rock = unreal.load_asset(tex.get('Rock', TEX_DEST + '/Rock'))

if sand:
    build_material('M_Ground', sand, 900)
else:
    unreal.log_error('no Sand texture')

if rock:
    # darkened so out-of-bounds terrain still reads as distinct from the ground
    build_material('M_Terrain', rock, 700, tint=(0.45, 0.42, 0.40))
else:
    unreal.log_error('no Rock texture')

unreal.log('=== level texture/material pass complete ===')
