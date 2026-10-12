"""Import the CC0 kit and build runtime materials in UE5.8's Python editor.

Run DownloadRealism.py first. Execute this script via UnrealEditor-Cmd with
-run=pythonscript -script=... -unattended -nullrhi. This rebuilds only materials
owned by this script under /Game/Realism, never gameplay source or maps.
"""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "AssetsSource"
MANIFEST = json.loads((SOURCE / "manifest.json").read_text())
DEST = "/Game/Realism"
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
ST = unreal.MaterialSamplerType
TEXTURES = {}
MATERIALS = {}


def node(material, kind, x=0, y=0, **props):
    value = EDIT.create_material_expression(material, getattr(unreal, "MaterialExpression" + kind), x, y)
    for key, prop in props.items():
        value.set_editor_property(key, prop)
    return value


def link(source, destination, pin="", output=""):
    if not EDIT.connect_material_expressions(source, output, destination, pin):
        raise RuntimeError("Material connection failed: " + output + " -> " + pin)


def output(source, property_name, pin=""):
    if not EDIT.connect_material_property(source, pin, getattr(MP, property_name)):
        raise RuntimeError("Material property connection failed: " + property_name)


def scalar(material, name, default, y=0):
    return node(material, "ScalarParameter", -800, y, parameter_name=name, default_value=default)


def tint(material, color=(1, 1, 1, 1)):
    return node(material, "VectorParameter", -800, -350, parameter_name="Tint", default_value=unreal.LinearColor(*color))


def multiply(material, a, b, x=-250, y=0):
    value = node(material, "Multiply", x, y)
    link(a, value, "A")
    link(b, value, "B")
    return value


def custom(m, code, names, output_type):
    expression = node(m, "Custom", -200, 500, code=code, output_type=output_type)
    inputs = []
    for name in names:
        value = unreal.CustomInput()
        value.set_editor_property("input_name", name)
        inputs.append(value)
    expression.set_editor_property("inputs", inputs)
    return expression


def material(name):
    path = DEST + "/" + name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if asset:
        EDIT.delete_all_material_expressions(asset)
    else:
        asset = ASSETS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if not asset:
        raise RuntimeError("Cannot create material " + name)
    # Reset controlled flags so rerunning after a shader change is deterministic.
    asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    asset.set_editor_property("two_sided", False)
    asset.set_editor_property("tangent_space_normal", True)
    asset.set_editor_property("is_sky", False)
    MATERIALS[name] = asset
    return asset


def save_material(asset):
    EDIT.set_base_material_usage(asset, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    EDIT.recompile_material(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Cannot save " + asset.get_name())


def import_textures():
    tasks = []
    state_file = SOURCE / "imported_texture_hashes.json"
    previous = json.loads(state_file.read_text()) if state_file.exists() else {}
    current = {}
    metadata = {item["local"]: item for item in MANIFEST["files"]}
    for name, filename in MANIFEST["textures"].items():
        current[name] = hashlib.md5((SOURCE / filename).read_bytes()).hexdigest()
        if previous.get(name) == current[name] and unreal.EditorAssetLibrary.does_asset_exist(DEST + "/T_" + name):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(SOURCE / filename))
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", "T_" + name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    ASSETS.import_asset_tasks(tasks)
    for name in MANIFEST["textures"]:
        texture = unreal.load_asset(DEST + "/T_" + name)
        if not texture:
            raise RuntimeError("Texture import failed: " + name)
        normal = name.endswith("_Normal")
        masks = name.endswith("_ARM") or name.endswith("_Alpha")
        texture.set_editor_property("srgb", not (normal or masks or name == "Environment"))
        resolution = metadata[MANIFEST["textures"][name]].get("resolution", "1k")
        texture.set_editor_property("max_texture_size", int(resolution.rstrip("k")) * 1024)
        if normal:
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            # Sources are the DX variant, so no green-channel inversion is needed.
            texture.set_editor_property("flip_green_channel", False)
        elif masks:
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        elif name == "Environment":
            if not isinstance(texture, unreal.TextureCube):
                raise RuntimeError("Environment HDR did not import as TextureCube")
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_HDR_COMPRESSED)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        TEXTURES[name] = texture
    state_file.write_text(json.dumps(current, indent=2))


def texture_node(m, name, object_only=False, y=0):
    sampler = ST.SAMPLERTYPE_NORMAL if name.endswith("_Normal") else ST.SAMPLERTYPE_MASKS if name.endswith(("_ARM", "_Alpha")) else ST.SAMPLERTYPE_LINEAR_COLOR if name == "Environment" else ST.SAMPLERTYPE_COLOR
    return node(m, "TextureObject" if object_only else "TextureSample", -800, y, texture=TEXTURES[name], sampler_type=sampler)


def world_texture(m, name, scale, normal=False, y=0):
    function_name = "WorldAlignedNormal" if normal else "WorldAlignedTexture"
    function = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/" + function_name)
    call = node(m, "MaterialFunctionCall", -500, y)
    if not call.set_material_function(function):
        raise RuntimeError("Could not load material function " + function_name)
    link(texture_node(m, name, True, y), call, "TextureObject")
    link(scale, call, "TextureSize")
    return call


def channel(m, source, component, pin="", y=0):
    mask = node(m, "ComponentMask", -250, y, r=component == "R", g=component == "G", b=component == "B", a=False)
    link(source, mask, output=pin)
    return mask


def pbr(label, world_size):
    m = material("M_" + label)
    m.set_editor_property("tangent_space_normal", False)
    scale = scalar(m, "WorldScale", world_size, 400)
    base = world_texture(m, label + "_BaseColor", scale, y=-200)
    normal = world_texture(m, label + "_Normal", scale, normal=True, y=200)
    arm = world_texture(m, label + "_ARM", scale, y=600)
    color = node(m, "Multiply", -180, -200)
    link(base, color, "A", "XYZ Texture")
    link(tint(m), color, "B")
    if label == "Rock":
        color = multiply(m, color, scalar(m, "AlbedoGain", .38, -500))
    elif label == "Gravel":
        macro = custom(m, "float2 p=P.xy*.00075; float n=sin(p.x+sin(p.y*1.1))*.5+sin(p.y*.77+cos(p.x*.85))*.3; return lerp(float3(.29,.38,.18),float3(.84,.68,.45),saturate(n*.55+.5));", ["P"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        link(node(m, "WorldPosition", -900, -500), macro, "P")
        color = multiply(m, color, macro)
    elif label == "Meadow":
        # Preserve the photographed blades and ground litter while maintaining a
        # greener meadow in distant mip levels. Broad variation breaks repetition.
        macro = custom(m, "float2 p=P.xy*.00058; float n=sin(p.x+sin(p.y*.91))*.5+sin(p.y*.69+cos(p.x*1.13))*.3; return lerp(float3(.40,.70,.30),float3(.68,.96,.54),saturate(n*.5+.5));", ["P"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        link(node(m, "WorldPosition", -900, -500), macro, "P")
        color = multiply(m, color, macro)
    roughness = multiply(m, channel(m, arm, "G", "XYZ Texture", 620), scalar(m, "Roughness", 1.0, 700))
    if label in ("Concrete", "Asphalt"):
        # Broad damp patches add darker diffuse and reflected highlights without
        # another texture, translucent layer, or expensive displacement.
        wet = custom(m, "float2 p=P.xy*.003; float n=sin(p.x+sin(p.y*.73))*.45+sin(p.y*1.4+cos(p.x*.62))*.3+sin(p.x*3.7+p.y*2.1)*.12; return smoothstep(.08,.58,n)*Amount;", ["P", "Amount"], unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        link(node(m, "WorldPosition", -950, -600), wet, "P")
        link(scalar(m, "Wetness", .65, -700), wet, "Amount")
        darken = custom(m, "return 1-W*.3;", ["W"], unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        link(wet, darken, "W")
        color = multiply(m, color, darken)
        mixed = custom(m, "return lerp(R,.11,W);", ["R", "W"], unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        link(roughness, mixed, "R")
        link(wet, mixed, "W")
        roughness = mixed
    output(color, "MP_BASE_COLOR")
    output(normal, "MP_NORMAL", "XYZ Texture")
    output(channel(m, arm, "R", "XYZ Texture", 450), "MP_AMBIENT_OCCLUSION")
    output(roughness, "MP_ROUGHNESS")
    output(channel(m, arm, "B", "XYZ Texture", 800), "MP_METALLIC")
    output(scalar(m, "Specular", .16 if label == "Rock" else .22 if label == "Meadow" else .5, 900), "MP_SPECULAR")
    save_material(m)


def metal(painted=False):
    m = material("M_PaintedMetal" if painted else "M_Metal")
    uv = multiply(m, node(m, "TextureCoordinate", -1100, 500), scalar(m, "UVScale", 1.0, 600))
    base, normal, arm = [texture_node(m, "Metal_" + part, y=i * 240) for i, part in enumerate(("BaseColor", "Normal", "ARM"))]
    for sample in (base, normal, arm):
        link(uv, sample, "UVs")
    if painted:
        # Paint retains fine weathering without multiplying the chosen paint
        # color by the source plate's very dark bare-metal albedo.
        modulation = custom(m, "return .7 + .3*dot(C.rgb,float3(.2126,.7152,.0722));", ["C"], unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        link(base, modulation, "C")
        output(multiply(m, modulation, tint(m, (.25, .36, .42, 1))), "MP_BASE_COLOR")
    else:
        output(multiply(m, base, tint(m)), "MP_BASE_COLOR")
    output(normal, "MP_NORMAL")
    output(multiply(m, channel(m, arm, "G"), scalar(m, "Roughness", .7)), "MP_ROUGHNESS")
    output(scalar(m, "Metallic", .08 if painted else .9), "MP_METALLIC")
    output(channel(m, arm, "R"), "MP_AMBIENT_OCCLUSION")
    save_material(m)


def smooth_material(name, color, roughness, specular=.5, translucent=False, opacity=1):
    m = material(name)
    output(tint(m, color), "MP_BASE_COLOR")
    output(scalar(m, "Roughness", roughness), "MP_ROUGHNESS")
    output(scalar(m, "Specular", specular, 160), "MP_SPECULAR")
    output(scalar(m, "Metallic", 0, 280), "MP_METALLIC")
    if translucent:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
        output(scalar(m, "Opacity", opacity, 400), "MP_OPACITY")
    return m


def special_materials():
    # Smooth glossy latex stays readable without expensive subsurface scattering.
    m = smooth_material("M_Latex", (.7, .09, .04, 1), .27, .5)
    save_material(m)
    m = smooth_material("M_Droplet", (.13, .4, .52, 1), .06, .7)
    output(multiply(m, tint(m, (.13, .4, .52, 1)), scalar(m, "Glow", .04, 520)), "MP_EMISSIVE_COLOR")
    save_material(m)
    m = smooth_material("M_Glass", (.62, .76, .79, 1), .08, .7, True, .20)
    save_material(m)
    # Two analytic waves perturb normals; no screen-space refraction or simulation.
    m = smooth_material("M_Water", (.015, .035, .035, 1), .12, .8, True, .42)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("screen_space_reflections", True)
    waves = custom(m, "float2 p = P.xy * .026; float2 g = float2(cos(p.x + T*.65) + .35*cos(p.y*.7-T*.4), sin(p.y*.83 + T*.52)); return normalize(float3(g * .045, 1));", ["P", "T"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    link(node(m, "WorldPosition", -850, 700), waves, "P")
    link(node(m, "Time", -850, 850), waves, "T")
    m.set_editor_property("tangent_space_normal", False)
    output(waves, "MP_NORMAL")
    save_material(m)
    m = material("M_Sky")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("is_sky", True)
    direction = node(m, "Multiply", -500, 150, const_b=-1.0)
    link(node(m, "CameraVectorWS", -850, 150), direction, "A")
    rotated = custom(m, "float a = Angle*.01745329252; float s=sin(a), c=cos(a); return float3(D.x*c-D.y*s,D.x*s+D.y*c,D.z);", ["D", "Angle"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    link(direction, rotated, "D")
    link(scalar(m, "SkyRotation", 75, 600), rotated, "Angle")
    sky = texture_node(m, "Environment")
    link(rotated, sky, "UVs")
    output(multiply(m, sky, scalar(m, "SkyBrightness", .7, 400)), "MP_EMISSIVE_COLOR")
    save_material(m)


def foliage(name, label, tint_color=(1, 1, 1, 1), alpha=True):
    m = material(name)
    base = texture_node(m, label + "_BaseColor")
    colored = multiply(m, base, tint(m, tint_color))
    output(colored, "MP_BASE_COLOR")
    output(scalar(m, "Roughness", .86), "MP_ROUGHNESS")
    output(scalar(m, "Specular", .24), "MP_SPECULAR")
    if alpha:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        m.set_editor_property("opacity_mask_clip_value", .42)
        output(texture_node(m, label + "_Alpha"), "MP_OPACITY_MASK", "R")
        output(multiply(m, colored, scalar(m, "Transmission", .55)), "MP_SUBSURFACE_COLOR")
        output(scalar(m, "TransmissionOpacity", .55), "MP_OPACITY")
    save_material(m)


def import_models():
    for name, source in MANIFEST["models"].items():
        file = SOURCE / source
        if not file.exists():
            unreal.log_warning("Optional model not prepared: " + str(file))
            continue
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.static_mesh_import_data.set_editor_property("combine_meshes", True)
        options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
        task = unreal.AssetImportTask()
        for key, value in {"filename": str(file), "destination_path": DEST, "destination_name": name, "automated": True, "replace_existing": True, "save": True, "options": options, "factory": unreal.FbxFactory()}.items():
            task.set_editor_property(key, value)
        ASSETS.import_asset_tasks([task])
        mesh = unreal.load_asset(DEST + "/" + name)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Static mesh import failed: " + name)
        for i, slot in enumerate(mesh.get_editor_property("static_materials")):
            label = str(slot.get_editor_property("material_slot_name")).lower()
            if name == "SM_Rock":
                selected = "M_Rock"
            elif name == "SM_Grass":
                selected = "M_Grass"
            else:
                selected = "M_TreeLeaf" if "lea" in label else "M_TreeBranch" if "branch" in label else "M_TreeBark"
            mesh.set_material(i, MATERIALS[selected])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        unreal.log("CLANKA_REALISM_MESH " + name + " bounds=" + str(mesh.get_bounds()))


import_textures()
for label, world_size in (("Concrete", 200), ("Asphalt", 600), ("Gravel", 180), ("Meadow", 200), ("Rock", 180)):
    pbr(label, world_size)
metal()
metal(painted=True)
special_materials()
foliage("M_TreeBark", "TreeBark", alpha=False)
foliage("M_TreeBranch", "TreeBranch", alpha=False)
foliage("M_TreeLeaf", "TreeLeaf")
foliage("M_Grass", "Grass", (.75, .83, .63, 1))
import_models()
unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True)
unreal.log("CLANKA_REALISM_CONTENT_READY")
