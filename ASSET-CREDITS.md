# Asset credits and provenance

The realism kit uses assets from **[Poly Haven](https://polyhaven.com)**, licensed under **[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)**. Poly Haven's [asset license](https://polyhaven.com/license) permits commercial use, modification, and redistribution. These assets do not imply an endorsement of Clanka.

The download helper is **Powered by Poly Haven**. It uses their [public API](https://github.com/Poly-Haven/Public-API) with an identifying user agent and follows the [API terms](https://github.com/Poly-Haven/Public-API/blob/master/ToS.md). The finished game runs entirely from local assets and makes no API requests.

| Source asset | Creators | Included use |
|---|---|---|
| [Concrete Floor 02](https://polyhaven.com/a/concrete_floor_02) | Rob Tuytel | 4K base color, DirectX normal, packed AO/roughness/metalness |
| [Aerial Asphalt 01](https://polyhaven.com/a/aerial_asphalt_01) | Rob Tuytel | 2K base color, DirectX normal, packed AO/roughness/metalness |
| [Gravel Floor](https://polyhaven.com/a/gravel_floor) | Matterfield; Jenelle van Heerden | 2K base color, DirectX normal, packed AO/roughness/metalness |
| [Leafy Grass](https://polyhaven.com/a/leafy_grass) | Charlotte Baglioni | 2K base color, DirectX normal, packed AO/roughness/metalness for meadow ground |
| [Rock Boulder Dry](https://polyhaven.com/a/rock_boulder_dry) | Dimitrios Savva; Rico Cilliers | 1K rock surface texture set |
| [Metal Plate 02](https://polyhaven.com/a/metal_plate_02) | Rob Tuytel | 2K weathered metal texture set |
| [Kloofendal 48d Partly Cloudy](https://polyhaven.com/a/kloofendal_48d_partly_cloudy) | Greg Zaal | 4K midday HDR environment, sky and reflections |
| [Rock 07](https://polyhaven.com/a/rock_07) | Jenelle van Heerden | Scanned rock geometry simplified to 2,500 triangles |
| [Island Tree 01](https://polyhaven.com/a/island_tree_01) | Rob Tuytel; Rico Cilliers | Tree geometry adapted to 9,634 triangles, including 2,600 intact leaf cards; 2K bark, branches and masked leaves |
| [Grass Medium 01](https://polyhaven.com/a/grass_medium_01) | Rob Tuytel; Rico Cilliers | One grass clump simplified to 1,200 triangles; 1K color and alpha |

Sources and licensing were checked on October 12, 2026. Downloaded files are verified against the API's size and MD5 values. `AssetsSource/manifest.json` records the exact download URLs and checksums for the local source kit; `AssetsSource/model_report.json` records simplified mesh statistics.

## Processing and performance choices

- `Tools/DownloadRealism.py` downloads approximately 263 MiB of original sources. They are excluded from Git via `AssetsSource/.gitignore` and can be downloaded again.
- Optional Blender preparation simplifies geometry, preserves UV/material slots, centers X/Y, and places each mesh's base at Z=0. Export heights: rock 180 cm, tree 800 cm, grass 55 cm. No source image pixels are edited.
- Tree wood is simplified separately from foliage. New leaf cards sample the original canopy positions and existing leaf atlas, preserving visible foliage that generic mesh decimation otherwise removes.
- `Tools/PrepareRealism.py` imports the kit into `/Game/Realism`. Color textures use sRGB; packed data and DirectX normals use linear sampling, with no extra green-channel flip. Unreal applies runtime texture compression and mipmaps.
- Concrete, asphalt, gravel, meadow and rock use world-aligned PBR materials with a `WorldScale` parameter in centimeters. Concrete and asphalt add procedural damp patches controlled by `Wetness`; gravel and meadow add broad color variation. Metal exposes `UVScale`, `Tint`, `Roughness` and `Metallic`. The selected metal source is weathered plate, not a brushed-metal scan.
- `M_PaintedMetal` reuses the metal normal and packed maps, with subtle source-luminance variation applied to the chosen paint color. This keeps painted surfaces readable while retaining weathering, without another texture set.
- Water, glass, glossy latex, droplets and the sky shader are original project materials. Water uses two inexpensive analytic normal waves and screen-space reflections; it does not simulate fluid dynamics. Glass and water omit screen-space refraction. Sky rotation is controlled in degrees with `SkyRotation` (default 75).
- Texture import checks source hashes and reimports changed files, with maximum texture dimensions matching the source resolutions above. Runtime streaming can select smaller mipmaps when appropriate.
- All materials support instanced static meshes. Foliage uses alpha testing and shared textures. These choices control asset cost; the actual frame rate must be measured on the target PC.

The existing synthesized audio was created specifically for this project by `Tools/GenerateAudio.py`; it does not contain samples from an external sound library.
