# Creates /Game/VFX/M_HitFlash: unlit additive fresnel flash used as a brief overlay
# when a character takes damage. Emissive = Fresnel(1.5, 0.55) * VectorParam "Color".
# Run headless:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
# Idempotent: deletes and recreates the asset if it already exists.
import unreal

PKG = "/Game/VFX"
NAME = "M_HitFlash"
PATH = PKG + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.EditorAssetLibrary.delete_asset(PATH)

tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary

mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
try:
    mat.set_editor_property("two_sided", True)
except Exception as e:
    unreal.log_warning("two_sided not set: %s" % e)

fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -350, 0)
fres.set_editor_property("exponent", 1.5)
fres.set_editor_property("base_reflect_fraction", 0.55)

col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -350, 180)
col.set_editor_property("parameter_name", "Color")
col.set_editor_property("default_value", unreal.LinearColor(3.0, 2.6, 2.2, 1.0))  # HDR warm white

mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -140, 60)
mel.connect_material_expressions(fres, "", mul, "A")
mel.connect_material_expressions(col, "", mul, "B")
mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(PATH)
print("CREATED M_HitFlash")
