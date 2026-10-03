"""Prepare Clanka assets in the Unreal editor (Python plugin required).

Example: UnrealEditor-Cmd Clanka.uproject -ExecutePythonScript=Tools/CreateContent.py
Run Tools/GenerateAudio.py first to regenerate the small original audio sources.
Existing material graphs and maps are preserved; required rendering usages are repaired.
"""
from pathlib import Path
import unreal

ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary
AUDIO_NAMES = ("S_Pop", "S_Splash", "S_Confirm", "S_Wave", "S_WaterLoop", "S_Ambience")


def scalar(material, parameter, default, x, y):
    expression = EDIT.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    expression.set_editor_property("parameter_name", parameter)
    expression.set_editor_property("default_value", default)
    return expression


def finish_material(material, path):
    if not material:
        raise RuntimeError("Could not load material " + path)
    # Water and confetti use UInstancedStaticMeshComponent. Enable this before
    # cooking, including on assets created by earlier bootstrap versions.
    usage = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
    EDIT.set_base_material_usage(material, usage, True)
    EDIT.recompile_material(material)
    if not EDIT.has_material_usage(material, usage):
        raise RuntimeError("Instanced static mesh usage was not enabled for " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)
    return material


def make_material(name, color, roughness, glow=0.0, translucent=False):
    path = "/Game/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return finish_material(unreal.load_asset(path), path)
    material = ASSETS.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create " + path)
    tint = EDIT.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(*color))
    EDIT.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar(material, "Roughness", roughness, -500, 180)
    EDIT.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if glow:
        strength = scalar(material, "Glow", glow, -500, -180)
        multiply = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -230, -120)
        EDIT.connect_material_expressions(tint, "", multiply, "A")
        EDIT.connect_material_expressions(strength, "", multiply, "B")
        EDIT.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if translucent:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("two_sided", True)
        opacity = scalar(material, "Opacity", 0.68, -500, 360)
        EDIT.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    if name == "M_Glow":
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    return finish_material(material, path)


def import_audio():
    source = Path(__file__).resolve().parents[1] / "Content" / "Audio" / "Source"
    tasks = []
    for name in AUDIO_NAMES:
        filename = source / (name + ".wav")
        if not filename.is_file():
            raise RuntimeError("Audio source missing: " + str(filename) + "; run Tools/GenerateAudio.py first")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(filename))
        task.set_editor_property("destination_path", "/Game/Audio")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    ASSETS.import_asset_tasks(tasks)
    for name in AUDIO_NAMES:
        sound = unreal.load_asset("/Game/Audio/" + name)
        if not sound or not isinstance(sound, unreal.SoundWave):
            raise RuntimeError("Audio import failed: " + name)
        sound.set_editor_property("looping", name in ("S_WaterLoop", "S_Ambience"))
        unreal.EditorAssetLibrary.save_loaded_asset(sound)


make_material("M_Splash", (0.10, 0.70, 0.80, 1.0), 0.32)
make_material("M_Glow", (0.03, 0.75, 1.0, 1.0), 0.30, glow=2.4)
make_material("M_Water", (0.06, 0.57, 0.95, 1.0), 0.15, glow=0.12, translucent=True)
import_audio()

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Maps/SplashLab"):
    if not level.new_level("/Game/Maps/SplashLab"):
        raise RuntimeError("Could not create /Game/Maps/SplashLab")
    if not level.save_current_level():
        raise RuntimeError("Could not save /Game/Maps/SplashLab")
unreal.log("CLANKA_CONTENT_READY: materials, original audio, and SplashLab map")
