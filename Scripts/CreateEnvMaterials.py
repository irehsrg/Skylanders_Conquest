# Stylized ground/terrain materials for the Joust map. Unlit (exact colors, no
# sun-wash) with world-space noise blending two shades, so the flat green ground
# reads as grass and the dark walls read as rock — no texture assets needed.
import unreal

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PKG = "/Game/Environment"


def make(name, col_dark, col_light, noise_scale):
    path = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = tools.create_asset(name, PKG, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    # World-space noise (Position defaults to absolute world position)
    noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -450, 0)
    noise.set_editor_property("scale", noise_scale)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)

    a = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -450, 200)
    a.set_editor_property("constant", col_dark)
    b = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -450, 340)
    b.set_editor_property("constant", col_light)

    lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -180, 120)
    mel.connect_material_expressions(a, "", lerp, "A")
    mel.connect_material_expressions(b, "", lerp, "B")
    mel.connect_material_expressions(noise, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    print("Created", name)


# Grass: two greens; medium patch size
make("M_Ground", unreal.LinearColor(0.10, 0.26, 0.11), unreal.LinearColor(0.22, 0.44, 0.17), 0.0022)
# Rock: two grays, tighter noise for a craggier look
make("M_Terrain", unreal.LinearColor(0.05, 0.06, 0.07), unreal.LinearColor(0.14, 0.15, 0.17), 0.0045)
