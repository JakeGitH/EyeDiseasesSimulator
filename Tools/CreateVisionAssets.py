"""Creates the editor assets the vision simulator needs from C++.

Run once (and again after deleting the assets) with the editor closed:

    "C:\\Program Files\\Epic Games\\UE_5.6\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
        "C:\\Dev\\EyeDiseasesSimulator\\EyeDiseasesSimulator.uproject" ^
        -run=pythonscript -script="C:\\Dev\\EyeDiseasesSimulator\\Tools\\CreateVisionAssets.py" -unattended -nosplash

or from the editor: Tools > Execute Python Script.

Assets:
    /Game/VisionSim/Materials/M_VisionFeed - unlit, two-sided material that shows the processed
    camera image (texture parameter "FeedTexture"), used by AVisionDisplayActor.
"""

import unreal

MATERIAL_DIR = "/Game/VisionSim/Materials"
MATERIAL_NAME = "M_VisionFeed"
TEXTURE_PARAMETER = "FeedTexture"
DEFAULT_TEXTURE = "/Engine/EngineResources/DefaultTexture.DefaultTexture"


def create_feed_material():
    path = f"{MATERIAL_DIR}/{MATERIAL_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f"{path} already exists - skipped")
        return

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", True)

    mel = unreal.MaterialEditingLibrary
    texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    texture.set_editor_property("parameter_name", TEXTURE_PARAMETER)
    texture.set_editor_property("texture", unreal.load_asset(DEFAULT_TEXTURE))
    texture.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_property(texture, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log(f"Created {path}")


create_feed_material()
