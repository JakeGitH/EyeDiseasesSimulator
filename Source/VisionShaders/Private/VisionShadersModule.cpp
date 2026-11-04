#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

/** Maps the project's Shaders folder to the virtual path /VisionShaders used by IMPLEMENT_GLOBAL_SHADER. */
class FVisionShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/VisionShaders"), ShaderDirectory);
	}
};

IMPLEMENT_MODULE(FVisionShadersModule, VisionShaders)
