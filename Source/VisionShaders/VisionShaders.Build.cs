// Shader module: global shaders and Render Dependency Graph passes for the vision effects.
// Loaded at PostConfigInit, because global shader types must be registered before the engine compiles shaders.
// Keep source files ASCII-only (Android clang rejects UTF-16 files that Unreal may create on re-save).

using UnrealBuildTool;

public class VisionShaders : ModuleRules
{
	public VisionShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"RenderCore",
			"RHI"
		});
	}
}
