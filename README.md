# Eye Diseases Simulator

**English** | [Polski](README.pl.md)

A Mixed Reality app for **Meta Quest 3 / 3S** that shows how people with vision impairments and eye diseases see the world.
You see your real surroundings through the headset cameras, and the image is **processed in real time** (filters, convolution, color matrices, masks) to simulate the selected condition.

Course project for *Image Processing*.

## Simulated conditions

| Condition | What you see | Image processing operations |
|---|---|---|
| Protanopia / Deuteranopia / Tritanopia | color blindness (red/green or blue/yellow confusion) | 3×3 matrix in linear RGB (Machado et al., 2009), interpolated by severity |
| Achromatopsia | no color vision, lower acuity, light sensitivity | luminance conversion (Rec. 709), blur, glare |
| Cataract | blur, haze, yellowing, glare around lights | Gaussian blur, blending with a haze color, contrast reduction, bright-pass filter + blur (glare) |
| Glaucoma | narrowed visual field (tunnel vision) | radial mask with a soft edge, peripheral blur |
| Macular degeneration (AMD) | dark, blurred spot in the center, wavy lines | central mask with a noisy edge, UV displacement (metamorphopsia) |
| Myopia / Hyperopia / Presbyopia | distant objects blurred (myopia) or near objects blurred (hyperopia, presbyopia) | **depth-dependent blur**: vergence map (1/distance) from the Depth API, thin-lens model, Gaussian blur with per-pixel sigma (interpolation of 3 levels) |
| Diabetic retinopathy | dark patches, blur | thresholded fractal noise (fBm), blur, contrast reduction |

Most conditions have a **severity** parameter (0–1). Refractive errors use physical units: **diopters** (e.g. −7 D, +4 D) and age (amplitude of accommodation).

### Depth-dependent blur (refractive errors)

1. **Depth map**: a 40×30 grid of rays from the camera through the image cell centers, raycast against Depth API data (MRUK `RaycastEnvironment`), 300 rays per frame (round robin). The result is stored as **vergence** V = 1/d [D] in an R16F texture and bilinearly upsampled in the shader.
2. **Focus range of the eye**: an eye with refractive error R [D] focuses vergence −R when relaxed; accommodation adds up to A diopters:
   `Vmin = −R`, `Vmax = −R + A`, where `A = max(0, 18.5 − 0.3·age)` (Hofstetter's formula).
   Far point = 1/Vmin, near point = 1/Vmax.
3. **Defocus** of a pixel: `ΔD = max(Vmin − V, V − Vmax, 0)`.
4. **Blur size**: the blur circle has an angular diameter `β = p·ΔD` (p = pupil diameter in m). A Gaussian with the same second moment as a disk of radius r has `σ = r/2`, so `σ_px = f_px · p · ΔD / 4` (f_px = camera focal length in pixels).
5. **Variable-sigma blur**: the GPU computes two levels (σ_max for the largest ΔD in the frame and 0.35·σ_max); each pixel interpolates linearly between the sharp image, the middle level and the maximum level.

Example (age 45 → A = 5 D): −3 D: sharp from 13 cm to 33 cm. +3 D: sharp from 50 cm to infinity. +6 D: nothing is sharp.

## How it works

```
Quest passthrough camera (Meta XR: Passthrough Camera Access)
        │  1280×960 frame, sRGB
        ▼
VisionShaders (HLSL, Render Dependency Graph)
   DownsamplePS   → 2^n× downscale  (+ bright-pass for glare)
   GaussianBlurPS → separable Gaussian convolution (horizontal, then vertical)
   CompositePS    → blur masks, depth-based blur, glare, color matrix,
                    haze, contrast, vignette, scotoma, distortion,
                    spots, compare mode
        ▲
        │ 40×30 vergence map (R16F)
UVisionDepthMap ← Depth API raycasts (MRUK)
        │
        ▼
Render target → quad in front of the eyes, placed using the camera parameters
(focal length, principal point, lens offset relative to the headset)
```

Key files:

| File | Role |
|---|---|
| `Shaders/Private/VisionEffects.usf` | all image processing algorithms (HLSL) |
| `Source/VisionShaders/` | shader module: shader classes and pass order (RDG) |
| `Source/EyeDiseasesSimulator/VisionConditionPresets.cpp` | parameters of each condition, optical model of refractive errors (`VisionOptics`) |
| `Source/EyeDiseasesSimulator/VisionDisplayActor.*` | camera, permissions, per-frame processing, display, blur per diopter |
| `Source/EyeDiseasesSimulator/VisionDepthMap.*` | depth (vergence) map from Depth API raycasts |
| `Source/EyeDiseasesSimulator/VisionSimulatorSubsystem.*` | simulator state (`FVisionSettings`), API for the menu |
| `Source/EyeDiseasesSimulator/HandMenuActor.*` | hand menu (generic, reusable in other MR projects) |
| `Source/EyeDiseasesSimulator/VisionMenuWidget.*` | menu logic (pages, Back, slider, Exit with confirmation) |
| `Source/EyeDiseasesSimulator/VisionHandMenuWidget.*` | menu layout and style built in C++ |
| `Source/EyeDiseasesSimulator/VisionConditionLibrary.*` | condition names and descriptions for the UI |
| `Tools/CreateVisionAssets.py` | creates the `M_VisionFeed` material in the editor |

## Requirements

- Unreal Engine **5.6**
- **Meta XR** plugin (tested with version 1.117), which includes Passthrough Camera Access
- Visual Studio 2022 with C++ support, Android SDK/NDK configured for UE
- Meta Quest 3 or 3S in developer mode (Horizon OS v74+)

## Getting started

1. Clone the repository with **Git LFS** (`git lfs install` before cloning).
2. Right-click `EyeDiseasesSimulator.uproject` → *Generate Visual Studio project files*, then open the project (this compiles the C++ modules).
3. Create the material that displays the image: in the editor, *Tools → Execute Python Script* → `Tools/CreateVisionAssets.py`.
4. Package the project for Android (*Platforms → Android → Package Project*) and install the APK on the headset.
5. On first launch, allow access to the headset cameras and to spatial data (needed for depth).

The boundary (Guardian) is disabled: `bBoundaryVisibilitySupportEnabled` + `bDefaultBoundaryVisibilitySuppressed` in `Config/DefaultEngine.ini` (Meta XR Boundary Visibility API, works only while passthrough is active). You can walk outside the play area without the app being interrupted.

> **Caution:** without the boundary nothing warns you about obstacles. Strong effects (cataract, glaucoma, AMD) deliberately limit your vision, so walk carefully.

## Controls

The app starts with normal vision. The **hand menu on the left hand** appears when you turn your palm toward your face and look at it.
Press buttons with the right index finger (poke) or with the hand ray.

- **Condition list**: selecting a condition turns it on and switches the menu page, so an accidental touch cannot switch to another condition.
- **Back**: turns the condition off and returns to the list.
- **Myopia / Hyperopia**: slider in diopters (−10…−0.25 D / +0.25…+6 D) with −/+ buttons, and the distance range in which the image is sharp.
- **Presbyopia**: age slider (40–65 years; above ~62 the accommodation amplitude by Hofstetter is 0).
- **Compare** (menu footer): left half of the image unprocessed, right half processed.
- **Exit** (menu footer): asks for confirmation (Cancel / Exit) before closing the app.

The menu (layout and style) is built in C++: `UVisionHandMenuWidget`. Optionally, `Content/VisionSim/UI/BP_HandMenu` (parent: `HandMenuActor`) can be used to tweak offsets in the editor or replace the widget.

### Console variables

| Variable | Default | Description |
|---|---|---|
| `vision.AutoCycleSeconds` | 0 | switch conditions automatically every N s (demo without the menu) |
| `vision.PupilDiameterMm` | 5 | pupil diameter, scales the refractive blur |
| `vision.DefocusScale` | 2 | blur multiplier (1 = physical; 2 compensates for camera and display resolution) |
| `vision.DebugView` | 0 | 1 = depth map, 2 = blur sigma (for documentation) |
| `vision.Depth.RaysPerFrame` | 300 | depth raycasts per frame |
| `vision.Depth.FallbackDistance` | 2 | distance [m] assumed when no depth data is available |
| `handmenu.Debug` | 0 | draws the palm normal and the detection state |

They can be set in `Config/DefaultEngine.ini` in the `[ConsoleVariables]` section.

## Status

- [x] Camera frame access (Passthrough Camera Access)
- [x] Image processing pipeline in shaders (blur, glare, color matrices, masks, noise)
- [x] Parameters for all conditions
- [x] Tested on the headset: camera image and effects work
- [ ] Performance measurement (target: 72 FPS)
- [ ] Stereo image (a separate camera for each eye)
- [x] Hand menu logic (palm detection, menu pages, sliders)
- [x] Menu look (C++, `UVisionHandMenuWidget`)
- [x] Depth-dependent blur (myopia, hyperopia, presbyopia)
- [x] Compare mode: normal / impaired
- [x] Boundary disabled, Exit button with confirmation

## References

- G. M. Machado, M. M. Oliveira, L. A. F. Fernandes, *A Physiologically-based Model for Simulation of Color Vision Deficiency*, IEEE TVCG 15(6), 2009.
- H. W. Hofstetter, *Useful age-amplitude formula*, Optometry World 38, 1950 (amplitude of accommodation as a function of age).
- Meta, *Depth API* / MR Utility Kit *Environment Raycasting*: Meta Horizon OS developer documentation.
- Meta, *Passthrough Camera API*: Meta Horizon OS developer documentation.
