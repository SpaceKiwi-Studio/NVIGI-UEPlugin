// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Security.Claims;
using UnrealBuildTool;

public class IGI : ModuleRules
{
    protected virtual bool IsSupportedTarget(ReadOnlyTargetRules Target)
    {
        return Target.Platform == UnrealTargetPlatform.Win64;
    }

    public IGI(ReadOnlyTargetRules Target) : base(Target)
	{
        if (!IsSupportedTarget(Target)) return;

#if UE_5_3_OR_LATER
        // UE 5.3 added the ability to override bWarningsAsErrors.
        // Treat all warnings as errors only on supported targets/engine versions >= 5.3.
        // Anyone else is in uncharted territory and should use their own judgment :-)
        bWarningsAsErrors = true;
#endif

        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
                Path.Combine([PluginDirectory, "ThirdParty", "nvigi_pack", "nvigi_core", "include"]),
                Path.Combine([PluginDirectory, "ThirdParty", "nvigi_pack", "plugins", "sdk", "include"]),
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
                Path.Combine(EngineDirectory,"Source/Runtime/D3D12RHI/Private"),
                Path.Combine(EngineDirectory,"Source/Runtime/D3D12RHI/Public/Windows"),
                Path.Combine(EngineDirectory,"Source/Runtime/VulkanRHI/Private"),
                Path.Combine(EngineDirectory,"Source/Runtime/VulkanRHI/Public"),
            }
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",
				"RHI",
				"D3D12RHI",
				"VulkanRHI"
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
                // ... add any modules that your module loads dynamically here ...
            }
        );

        PublicDefinitions.Add("AIM_CORE_BINARY_NAME=TEXT(\"nvigi.core.framework.dll\")");

        string PluginsBinaryPath = Path.Combine([PluginDirectory, "ThirdParty", "nvigi_pack", "plugins", "sdk", "bin", "x64"]);
        string GPTModelPath = Path.Combine([PluginDirectory, "ThirdParty", "nvigi_pack", "plugins", "sdk", "data", "nvigi.models", "nvigi.plugin.gpt.ggml", "{8E31808B-C182-4016-9ED8-64804FF5B40D}"]);

        // Core framework DLL
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "nvigi.core.framework.dll"));

        // GPT feature + dependencies
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "nvigi.plugin.gpt.ggml.cuda.dll"));
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "cig_scheduler_settings.dll"));
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "cublas64_12.dll"));
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "cublasLt64_12.dll"));
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "cudart64_12.dll"));
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "nvigi.plugin.hwi.common.dll"));
        RuntimeDependencies.Add(Path.Combine(PluginsBinaryPath, "nvigi.plugin.hwi.cuda.dll"));

        RuntimeDependencies.Add(Path.Combine(GPTModelPath, "nemotron-4-mini-4b-instruct_q4_0.gguf"));
        RuntimeDependencies.Add(Path.Combine(GPTModelPath, "nvigi.model.config.json"));

        AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
    }
}
