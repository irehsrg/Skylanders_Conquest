# Creates the runtime VFX materials used by the character HUD/targeting:
#   /Game/VFX/M_Highlight    - additive fresnel glow, used as a SetOverlayMaterial
#                              highlight on the enemy under the auto-attack aim
#   /Game/VFX/M_AimTargeter  - unlit TRANSLUCENT material with Color + Opacity
#                              params; the ground aim line + circle use it so the
#                              targeter is genuinely see-through and never blocks
#                              the view (the old opaque BasicShapeMaterial did).
import unreal

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PKG = "/Game/VFX"


def fresh(name):
    path = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return tools.create_asset(name, PKG, unreal.Material, unreal.MaterialFactoryNew()), path


# ---- M_Highlight (additive fresnel glow) ----
mat, path = fresh("M_Highlight")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -350, 0)
fres.set_editor_property("exponent", 2.0)
fres.set_editor_property("base_reflect_fraction", 0.35)
col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 180)
col.set_editor_property("constant", unreal.LinearColor(0.35, 2.4, 2.9, 1.0))
mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -140, 60)
mel.connect_material_expressions(fres, "", mul, "A")
mel.connect_material_expressions(col, "", mul, "B")
mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
print("Created M_Highlight")

# ---- M_MapFloor (unlit, flat color) ----
# Greybox map floors/walls use this so their exact color shows regardless of
# lighting (the lit BasicShapeMaterial washed every tint to the same gray-green).
mat, path = fresh("M_MapFloor")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
colp = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -350, 0)
colp.set_editor_property("parameter_name", "Color")
colp.set_editor_property("default_value", unreal.LinearColor(0.1, 0.3, 0.1, 1.0))
mel.connect_material_property(colp, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
print("Created M_MapFloor")

# ---- M_AimTargeter (translucent, parameterized) ----
mat, path = fresh("M_AimTargeter")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
colp = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -350, 0)
colp.set_editor_property("parameter_name", "Color")
colp.set_editor_property("default_value", unreal.LinearColor(1.0, 0.85, 0.0, 1.0))
opp = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 200)
opp.set_editor_property("parameter_name", "Opacity")
opp.set_editor_property("default_value", 0.35)
mel.connect_material_property(colp, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(opp, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
print("Created M_AimTargeter")
