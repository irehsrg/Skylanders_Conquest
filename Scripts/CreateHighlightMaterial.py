# Creates /Game/VFX/M_Highlight — an unlit additive fresnel-rim material used as
# a SetOverlayMaterial highlight on the enemy currently under the auto-attack aim.
import unreal

PKG = "/Game/VFX"
NAME = "M_Highlight"
PATH = PKG + "/" + NAME

# Force a clean rebuild (a prior partial create may have left a broken asset)
if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.EditorAssetLibrary.delete_asset(PATH)

if True:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)

    # Lower exponent + higher base fraction so the whole silhouette glows, not
    # just a thin rim; HDR (>1) color so the additive overlay reads brightly.
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -350, 0)
    fres.set_editor_property("exponent", 2.0)
    fres.set_editor_property("base_reflect_fraction", 0.35)

    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 180)
    col.set_editor_property("constant", unreal.LinearColor(0.35, 2.4, 2.9, 1.0))  # bright cyan glow

    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -140, 60)
    mel.connect_material_expressions(fres, "", mul, "A")
    mel.connect_material_expressions(col, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    print("Created M_Highlight")
