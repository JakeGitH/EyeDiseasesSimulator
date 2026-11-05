// MR Quest 3 template - main game module.
// When a project is created from the template, the module name is replaced with the project name.
// Keep template source files ASCII-only: Unreal re-saves files with non-ASCII characters as UTF-16,
// which the Android (clang) compiler rejects.

using UnrealBuildTool;

public class EyeDiseasesSimulator : ModuleRules
{
	public EyeDiseasesSimulator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",

			// XR (Motion Controller, HMD)
			"HeadMountedDisplay",
			"XRBase"
		});

		// Module names verified in Engine/Plugins/Marketplace/MetaXR and MetaXRInteraction/Source.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OculusInteraction",          // ISDK: grabbable, poke, ray
			"OculusInteractionPrebuilts", // ISDK: hand rigs and hand visuals
			"OculusXRHMD",                // EOculusXRPassthroughLayerOrder, passthrough layer shapes
			"OculusXRPassthrough",        // UOculusXRPassthroughSubsystem (persistent passthrough)

			// Vision simulator
			"VisionShaders",              // image processing passes (global shaders)
			"MRUtilityKit",               // UMRUKPassthroughCameraAccess: passthrough camera frames; environment raycasts (depth)
			"OculusXRAnchors",            // included by the MRUK subsystem headers
			"AndroidPermission",          // runtime request of horizonos.permission.HEADSET_CAMERA
			"ProceduralMeshComponent",    // camera feed quad built from the camera intrinsics
			"UMG",                        // hand menu widget
			"Slate",
			"SlateCore",
			"RenderCore",
			"RHI"
		});

		// Uncomment when you start using these features from C++.
		// PrivateDependencyModuleNames.AddRange(new string[]
		// {
		// 	"OculusXRAnchors",     // spatial anchors
		// 	"OculusXRScene",       // scene / room
		// 	"MRUtilityKit"         // Meta MR Utility Kit (requires the MRUK plugin enabled)
		// });
	}
}
