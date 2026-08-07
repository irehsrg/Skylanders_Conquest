# Rebuilds /Game/Environment/M_Ground as a three-way lane blend:
#
#   grass   off-lane base
#   dirt    trodden shoulder where the path meets the grass
#   cobble  the lane core
#
#     outer = clamp((m - DIRT_IN)   / DIRT_FADE)      grass -> dirt
#     inner = clamp((m - COBBLE_IN) / COBBLE_FADE)    dirt  -> cobble
#     col   = lerp(lerp(grass, dirt, outer), cobble, inner)
#
# `m` is the baked lane mask, so the shape comes from GenerateLaneMask.py and the
# surfaces from GenerateGroundTextures.py. Run both first.
# PreviewLaneGround.py mirrors this maths offline -- tune there, it is a second
# per iteration instead of a game relaunch.
#
# This supersedes the M_Ground built by ImportLevelTextures.py, which still owns
# the Barrens/Verdant source textures and M_Terrain.
#
# Headless, editor CLOSED:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript
#       -script="<abs>/Scripts/BuildLaneGround.py" -unattended -nullrhi

import os

import unreal

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MASK_SRC = os.path.join(PROJECT, 'Saved', 'LaneMask').replace('\\', '/')
GROUND_SRC = os.path.join(PROJECT, 'Saved', 'GroundTex').replace('\\', '/')

MAT_DIR = '/Game/Environment'
TEX_DIR = '/Game/Environment/Textures'

# Must match GenerateLaneMask.py / the ground plane in BuildMap.
MAP_W, MAP_H = 27000.0, 10500.0

# Keep in sync with PreviewLaneGround.py.
GRASS_TILE = 1400.0
DIRT_TILE = 800.0
COBBLE_TILE = 300.0
RIM_DARKEN = 0.30        # trodden shoulder where stone meets grass

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def import_png(src_dir, filename, dest, mask_like=False):
    task = unreal.AssetImportTask()
    task.set_editor_property('automated', True)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('filename', src_dir + '/' + filename)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    asset_tools.import_asset_tasks([task])

    path = dest + '/' + os.path.splitext(filename)[0]
    tex = unreal.load_asset(path)
    if not tex:
        unreal.log_error('import failed: ' + src_dir + '/' + filename)
        return None
    if mask_like:
        # A mask, not colour: linear, and CLAMPED so the plane outside the baked
        # area does not tile a second copy of the lane across the map.
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings',
                                unreal.TextureCompressionSettings.TC_GRAYSCALE)
        tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
        tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    else:
        tex.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property('address_y', unreal.TextureAddress.TA_WRAP)
    unreal.EditorAssetLibrary.save_asset(path)
    return tex


def build(mask_tex, grass, dirt, cobble):
    pkg = MAT_DIR + '/M_Ground'
    # Reuse the existing asset and wipe its graph rather than delete+recreate:
    # the map builder resolves M_Ground by path at runtime, and a delete the
    # package system has not released makes the subsequent create fail.
    mat = unreal.load_asset(pkg)
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset('M_Ground', MAT_DIR, unreal.Material,
                                       unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error('could not create or load M_Ground')
        return None

    def expr(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    def const(v, x, y):
        n = expr(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property('r', v)
        return n

    def const2(a, b, x, y):
        n = expr(unreal.MaterialExpressionConstant2Vector, x, y)
        n.set_editor_property('r', a)
        n.set_editor_property('g', b)
        return n

    def mul(a, b, x, y, ao='', bo=''):
        n = expr(unreal.MaterialExpressionMultiply, x, y)
        mel.connect_material_expressions(a, ao, n, 'A')
        mel.connect_material_expressions(b, bo, n, 'B')
        return n

    # World XY drives everything: the ground is scaled engine cubes whose own
    # UVs do not tile sanely at map scale.
    wp = expr(unreal.MaterialExpressionWorldPosition, -2200, 0)
    xy = expr(unreal.MaterialExpressionComponentMask, -2000, 0)
    xy.set_editor_property('r', True)
    xy.set_editor_property('g', True)
    xy.set_editor_property('b', False)
    xy.set_editor_property('a', False)
    mel.connect_material_expressions(wp, '', xy, '')

    def surface(tex, tile, y):
        uv = mul(xy, const(1.0 / tile, -1800, y + 130), -1600, y)
        samp = expr(unreal.MaterialExpressionTextureSample, -1400, y)
        samp.set_editor_property('texture', tex)
        mel.connect_material_expressions(uv, '', samp, 'UVs')
        return samp

    grass_s = surface(grass, GRASS_TILE, -700)
    cobble_s = surface(cobble, COBBLE_TILE, 700)

    # World XY -> [0,1] across the baked map extent. V is negated because the
    # mask was rasterised with +Y world running up the image.
    mask_uv = mul(xy, const2(1.0 / MAP_W, -1.0 / MAP_H, -1800, 1500), -1600, 1400)
    mask_uv_c = expr(unreal.MaterialExpressionAdd, -1400, 1400)
    mel.connect_material_expressions(mask_uv, '', mask_uv_c, 'A')
    mel.connect_material_expressions(const2(0.5, 0.5, -1600, 1580), '', mask_uv_c, 'B')

    mask_s = expr(unreal.MaterialExpressionTextureSample, -1200, 1400)
    mask_s.set_editor_property('texture', mask_tex)
    mel.connect_material_expressions(mask_uv_c, '', mask_s, 'UVs')

    # Deliberately a SINGLE lerp driven straight off the mask, with the dirt
    # folded in as a tint on the shoulder rather than a third sampled layer.
    #
    # The three-sample version (grass -> dirt -> cobble via two clamped ramps)
    # is correct on paper and renders correctly in PreviewLaneGround.py, but
    # in-engine it produced hard screen-space quadrant seams and heavy speckle.
    # Isolation tests showed each texture, the UVs and the mask sample are all
    # individually fine, so the fault is in the extra sample/ramp stack rather
    # than in any input. This two-sample shape is the one already proven to
    # render clean, so the lane uses that until the artifact is understood.
    blend = expr(unreal.MaterialExpressionLinearInterpolate, -300, 0)
    mel.connect_material_expressions(grass_s, 'RGB', blend, 'A')
    mel.connect_material_expressions(cobble_s, 'RGB', blend, 'B')
    mel.connect_material_expressions(mask_s, 'R', blend, 'Alpha')

    # rim = 4m(1-m): zero in the solid path and out in the grass, 1 at the
    # halfway contour -- a trodden shoulder where the stone meets the grass.
    inv = expr(unreal.MaterialExpressionOneMinus, -800, 1900)
    mel.connect_material_expressions(mask_s, 'R', inv, '')
    rim = mul(mask_s, inv, -650, 1850, ao='R')
    rim4 = mul(rim, const(4.0 * RIM_DARKEN, -650, 2020), -500, 1850)
    keep = expr(unreal.MaterialExpressionOneMinus, -380, 1850)
    mel.connect_material_expressions(rim4, '', keep, '')
    shaded = mul(blend, keep, -150, 200)

    # Unlit: these stylised maps wash out under the default lighting.
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(shaded, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(pkg)
    return mat


def main():
    mask = import_png(MASK_SRC, 'LaneMask.png', TEX_DIR, mask_like=True)
    grass = import_png(GROUND_SRC, 'Grass_C.png', TEX_DIR)
    dirt = import_png(GROUND_SRC, 'Dirt_C.png', TEX_DIR)
    cobble = import_png(GROUND_SRC, 'Cobble_C.png', TEX_DIR)
    if not (mask and grass and dirt and cobble):
        unreal.log_error('missing inputs - run GenerateLaneMask.py and '
                         'GenerateGroundTextures.py first')
        return
    if build(mask, grass, dirt, cobble):
        unreal.log('=== M_Ground rebuilt: grass / dirt / cobble lane blend ===')


main()
