"""Download a compact CC0 realism kit. Powered by Poly Haven: https://polyhaven.com

Uses the public API with an identifying user agent and checks every file's MD5.
Raw sources remain in AssetsSource (ignored). No image pixels are modified.
Run with normal Python; optionally pass --blender <blender.exe> to simplify models.
"""
from pathlib import Path
import argparse
import concurrent.futures
import hashlib
import http.client
import json
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "AssetsSource"
UA = "ClankaSplashLab-RealismKit/1.0 (Powered by Poly Haven)"
SETS = {
    "Concrete": ("concrete_floor_02", "4k"),
    "Asphalt": ("aerial_asphalt_01", "2k"),
    "Gravel": ("gravel_floor", "2k"),
    "Meadow": ("leafy_grass", "2k"),
    "Rock": ("rock_boulder_dry", "1k"),
    "Metal": ("metal_plate_02", "2k"),
}
MODELS = {"SM_Rock": "rock_07", "SM_Tree": "island_tree_01", "SM_Grass": "grass_medium_01"}


def get_json(asset):
    cache = DEST / (asset + ".json")
    if cache.exists():
        return json.loads(cache.read_text())
    request = urllib.request.Request("https://api.polyhaven.com/files/" + asset, headers={"User-Agent": UA})
    with urllib.request.urlopen(request, timeout=60) as response:
        data = json.load(response)
    cache.write_text(json.dumps(data, indent=2))
    return data


def entry(asset, key, resolution, preferred="jpg"):
    options = get_json(asset)[key][resolution]
    return options.get(preferred) or options["png"]


def download(item):
    target = DEST / item["local"]
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and hashlib.md5(target.read_bytes()).hexdigest() == item["md5"]:
        return
    request = urllib.request.Request(item["url"], headers={"User-Agent": UA})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                data = response.read()
            break
        except (OSError, http.client.HTTPException):
            if attempt == 2:
                raise
            time.sleep(attempt + 1)
    if len(data) != item["size"] or hashlib.md5(data).hexdigest() != item["md5"]:
        raise RuntimeError("Download integrity check failed: " + item["local"])
    target.write_bytes(data)
    print("Downloaded", item["local"], len(data), flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--blender", default="")
    args = parser.parse_args()
    DEST.mkdir(parents=True, exist_ok=True)
    (DEST / ".gitignore").write_text("*\n!.gitignore\n")
    manifest = {"provider": "Poly Haven", "license": "CC0-1.0", "textures": {}, "models": {}, "files": []}

    def add(asset, key, resolution, name, preferred="jpg"):
        source = dict(entry(asset, key, resolution, preferred))
        extension = Path(source["url"]).suffix
        source["local"] = "Textures/" + name + extension
        source["asset"] = asset
        source["channel"] = key
        source["resolution"] = resolution
        manifest["files"].append(source)
        manifest["textures"][name] = source["local"]

    for label, (asset, resolution) in SETS.items():
        add(asset, "Diffuse", resolution, label + "_BaseColor")
        add(asset, "nor_dx", resolution, label + "_Normal", "png")
        add(asset, "arm", resolution, label + "_ARM")
    add("kloofendal_48d_partly_cloudy", "hdri", "4k", "Environment", "hdr")

    for label, asset in MODELS.items():
        source = dict(get_json(asset)["fbx"]["1k"]["fbx"])
        source.pop("include", None)
        source["local"] = "Models/" + label + "_source.fbx"
        source["asset"] = asset
        manifest["files"].append(source)
        manifest["models"][label] = "Models/" + label + ".fbx"
    for prefix, key in (("TreeBark", "Diffuse"), ("TreeBranch", "branches_diff"), ("TreeLeaf", "leaves_diff")):
        add("island_tree_01", key, "2k", prefix + "_BaseColor")
    add("island_tree_01", "leaves_alpha", "2k", "TreeLeaf_Alpha", "png")
    add("grass_medium_01", "Diffuse", "1k", "Grass_BaseColor")
    add("grass_medium_01", "Alpha", "1k", "Grass_Alpha", "png")
    total = sum(x["size"] for x in manifest["files"])
    if total > 300 * 1024 * 1024:
        raise RuntimeError("Curated download exceeded 300 MiB budget")
    print("Powered by Poly Haven | CC0 asset kit |", round(total / 1024 / 1024, 1), "MiB", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(download, manifest["files"]))
    (DEST / "manifest.json").write_text(json.dumps(manifest, indent=2))
    if args.blender:
        preparation = DEST / "prepare_models.py"
        preparation.write_text(BLENDER_PREPARE)
        subprocess.run([args.blender, "--background", "--factory-startup", "--python", str(preparation), "--", str(DEST)], check=True)
    print("CLANKA_REALISM_DOWNLOAD_READY", flush=True)


BLENDER_PREPARE = r'''
import bpy, json, pathlib, sys, random, math
from mathutils import Vector
root = pathlib.Path(sys.argv[sys.argv.index("--") + 1])
report = {}
for name, target_triangles, height in [("SM_Rock", 2500, 1.8), ("SM_Tree", 9500, 8.0), ("SM_Grass", 1200, .55)]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root / "Models" / (name + "_source.fbx")))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if name == "SM_Grass":
        # The source is a collection of variants. Keep one dense clump.
        chosen = max(meshes, key=lambda o: len(o.data.polygons))
        for obj in meshes:
            if obj != chosen:
                bpy.data.objects.remove(obj, do_unlink=True)
        meshes = [chosen]
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.hide_set(False)
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    coords = [v.co for v in obj.data.vertices]
    lo = Vector(tuple(min(v[i] for v in coords) for i in range(3)))
    hi = Vector(tuple(max(v[i] for v in coords) for i in range(3)))
    offset = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    factor = height / max(.001, hi.z - lo.z)
    for vertex in obj.data.vertices:
        vertex.co = (vertex.co - offset) * factor
    if name == "SM_Tree":
        # A global decimator deletes tiny isolated leaves before large branches.
        # Replace them with intact photo-atlas cards across the original canopy,
        # and spend separate budgets on trunk and branches.
        rng = random.Random(7319)
        leaf_index = next(i for i,m in enumerate(obj.data.materials) if "leaves" in m.name)
        faces = [p for p in obj.data.polygons if p.material_index == leaf_index]
        picked = rng.sample(faces, min(2600, len(faces)))
        atlas = [(0,.49,.16,1),(.14,.48,.36,1),(.35,.47,.52,1),(.53,.49,.70,1),(.70,.49,.84,1),(.04,0,.26,.48),(.28,0,.52,.48),(.53,0,.70,.48)]
        verts, quads, uvs = [], [], []
        for face in picked:
            center = sum((obj.data.vertices[i].co for i in face.vertices), Vector()) / len(face.vertices)
            normal = face.normal.normalized()
            if normal.length < .1: normal = Vector((0,0,1))
            u = normal.cross(Vector((0,0,1)))
            if u.length < .1: u = normal.cross(Vector((0,1,0)))
            u.normalize()
            v = normal.cross(u).normalized()
            w, h = rng.uniform(.15,.30), rng.uniform(.30,.58)
            start = len(verts)
            verts.extend([center-u*w/2-v*h/2,center+u*w/2-v*h/2,center+u*w/2+v*h/2,center-u*w/2+v*h/2])
            quads.append(tuple(range(start,start+4)))
            x0,y0,x1,y1 = rng.choice(atlas)
            uvs.extend([(x0,y0),(x1,y0),(x1,y1),(x0,y1)])
        leaf_data = bpy.data.meshes.new("CanopyCards")
        leaf_data.from_pydata(verts, [], quads)
        leaf_data.materials.append(obj.data.materials[leaf_index])
        uv = leaf_data.uv_layers.new(name="UVMap")
        for index, coord in enumerate(uvs): uv.data[index].uv = coord
        canopy = bpy.data.objects.new("CanopyCards",leaf_data)
        bpy.context.collection.objects.link(canopy)
        # Separate source material sections before simplification.
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.separate(type="MATERIAL")
        bpy.ops.object.mode_set(mode="OBJECT")
        wood = []
        for part in list(bpy.context.scene.objects):
            if part.type != "MESH" or part == canopy: continue
            slots = [m.name for m in part.data.materials if m]
            used = {part.data.polygons[i].material_index for i in range(len(part.data.polygons))}
            names = [part.data.materials[i].name for i in used]
            if any("leaves" in s for s in names):
                bpy.data.objects.remove(part, do_unlink=True)
                continue
            bpy.ops.object.select_all(action="DESELECT")
            part.select_set(True); bpy.context.view_layer.objects.active = part
            tris = sum(len(p.vertices)-2 for p in part.data.polygons)
            budget = 2400 if any("branch" in s for s in names) else 1200
            if tris > budget:
                dec = part.modifiers.new("Wood budget", "DECIMATE")
                dec.ratio = budget / tris; dec.use_collapse_triangulate = True
                bpy.ops.object.modifier_apply(modifier=dec.name)
            wood.append(part)
        bpy.ops.object.select_all(action="DESELECT")
        for part in wood + [canopy]: part.select_set(True)
        bpy.context.view_layer.objects.active = wood[0]
        bpy.ops.object.join()
        obj = bpy.context.object
    triangles = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if triangles > target_triangles and name != "SM_Tree":
        mod = obj.modifiers.new("Distance-friendly mesh budget", "DECIMATE")
        mod.ratio = target_triangles / triangles
        mod.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for face in obj.data.polygons:
        face.use_smooth = True
    obj.name = name
    output = root / "Models" / (name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True, object_types={"MESH"}, add_leaf_bones=False, bake_anim=False, path_mode="STRIP", axis_forward="-Z", axis_up="Y")
    report[name] = {"triangles": sum(len(p.vertices)-2 for p in obj.data.polygons), "height_cm": height * 100, "materials": [m.name for m in obj.data.materials], "bytes": output.stat().st_size}
    print("CLANKA_MODEL", name, report[name], flush=True)
(root / "model_report.json").write_text(json.dumps(report, indent=2))
'''

if __name__ == "__main__":
    main()
