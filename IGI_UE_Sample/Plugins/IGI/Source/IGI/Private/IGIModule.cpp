// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "IGIModule.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#include "IGICore.h"
#include "IGIGPT.h"
#include "IGILog.h"

#include "nvigi.h"
#include "nvigi_ai.h"

#define LOCTEXT_NAMESPACE "FIGIModule"

class FIGIModule::Impl
{
public:
    Impl() {}

    virtual ~Impl() {}

    void StartupModule()
    {
        // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

        FString BaseDir = IPluginManager::Get().FindPlugin("IGI")->GetBaseDir();
        IGICoreLibraryPath = FPaths::Combine(*BaseDir, TEXT("ThirdParty/nvigi_pack/plugins/sdk/bin/x64/nvigi.core.framework.dll"));
        IGIModelsPath = FPaths::Combine(*BaseDir, TEXT("ThirdParty/nvigi_pack/plugins/sdk/data/nvigi.models"));
    }

    void ShutdownModule()
    {
        // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
        // we call this function before unloading the module.

        if (Core)
        {
            UnloadIGICore();
        }
    }

    bool LoadIGICore()
    {
        FScopeLock Lock(&CS);

        Core = MakeUnique<FIGICore>(IGICoreLibraryPath);
        return (Core != nullptr) && (Core->IsInitialized());
    }

    bool UnloadIGICore()
    {
        FScopeLock Lock(&CS);

        GPT.Reset();
        Core.Reset();
        return true;
    }

    nvigi::Result LoadIGIFeature(const nvigi::PluginID& Feature, nvigi::InferenceInterface** Interface, const UTF8CHAR* UTF8PathToPlugin = nullptr)
    {
        FScopeLock Lock(&CS);

        return Core->LoadInterface(Feature, nvigi::InferenceInterface::s_type, Interface, UTF8PathToPlugin);
    }

    nvigi::Result UnloadIGIFeature(nvigi::PluginID Feature, nvigi::InferenceInterface* Interface)
    {
        FScopeLock Lock(&CS);

        return Core->UnloadInterface(Feature, Interface);
    }

    // Get the D3D12 parameters
    nvigi::D3D12Parameters GetD3D12Parameters() const
    {
        nvigi::D3D12Parameters Parameters;
        
        if (!GDynamicRHI && GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12)
        {
            UE_LOG(LogIGISDK, Log, TEXT("UE not using D3D12; cannot use CiG"));
            return {};
        }

        ID3D12DynamicRHI* RHI = static_cast<ID3D12DynamicRHI*>(GDynamicRHI);

        if (!RHI)
        {
            UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve RHI instance from UE; cannot use CiG"));
            return {};
        }
        UE_LOG(LogIGISDK, Log, TEXT("RHI Vulkan parameters: %s"), RHI->GetName());

        ID3D12CommandQueue* CmdQ = RHI->RHIGetCommandQueue();
        constexpr uint32 RHI_DEVICE_INDEX = 0u;
        ID3D12Device* D3D12Device = RHI->RHIGetDevice(RHI_DEVICE_INDEX);

        if (!CmdQ || !D3D12Device)
        {
            UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve D3D12 device and command queue from UE; cannot use CiG"));
            return {};
        }

        Parameters.device = D3D12Device;
        Parameters.queue = CmdQ;

        return Parameters;
    }

    // Get the Vulkan parameters
    nvigi::VulkanParameters GetVulkanParameters() const
    {
        nvigi::VulkanParameters Parameters;
        
        if (!GDynamicRHI && GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::Vulkan)
        {
            UE_LOG(LogIGISDK, Log, TEXT("UE not using VULKAN; cannot use CiG"));
            return {};
        }

        IVulkanDynamicRHI* RHI = static_cast<IVulkanDynamicRHI*>(GDynamicRHI);

        if (!RHI)
        {
            UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve RHI instance from UE; cannot use CiG"));
            return {};
        }
        UE_LOG(LogIGISDK, Log, TEXT("RHI Vulkan parameters: %s"), RHI->GetName());

        VkQueue VkQ = RHI->RHIGetGraphicsVkQueue();
        VkDevice VkDevice = RHI->RHIGetVkDevice();

        if (!VkQ || !VkDevice)
        {
            UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve VULKAN device and command queue from UE; cannot use CiG"));
            return {};
        }

        Parameters.device = VkDevice;
        Parameters.queue = VkQ;

        return Parameters;
    }

    const FString GetModelsPath() const { return IGIModelsPath; }

    FIGIGPT* GetGPT(FIGIModule* module)
    {
        FScopeLock Lock(&CS);
        if(!GPT.IsValid())
        {
            GPT = MakeUnique<FIGIGPT>(module);
        }
        return GPT.Get();
    }

private:
    TUniquePtr<FIGICore> Core;
    TUniquePtr<FIGIGPT> GPT;

    FCriticalSection CS;
    FString IGICoreLibraryPath;
    FString IGIModelsPath;
};

// ----------------------------------

void FIGIModule::StartupModule()
{
    Pimpl = MakePimpl<FIGIModule::Impl>();
    Pimpl->StartupModule();
    UE_LOG(LogIGISDK, Log, TEXT("IGI module started"));
}

void FIGIModule::ShutdownModule()
{
    Pimpl->ShutdownModule();
    Pimpl.Reset();
    UE_LOG(LogIGISDK, Log, TEXT("IGI module shut down"));
}

bool FIGIModule::LoadIGICore()
{
    const bool Result{ Pimpl->LoadIGICore() };
    if (Result)
    {
        UE_LOG(LogIGISDK, Log, TEXT("IGI core loaded"));
    }
    else
    {
        UE_LOG(LogIGISDK, Fatal, TEXT("ERROR when loading IGI core"));
    }
    return Result;
}

bool FIGIModule::UnloadIGICore()
{
    const bool Result{ Pimpl->UnloadIGICore() };
    if (Result)
    {
        UE_LOG(LogIGISDK, Log, TEXT("IGI core unloaded"));
    }
    else
    {
        UE_LOG(LogIGISDK, Fatal, TEXT("ERROR when unloading IGI core"));
    }
    return Result;
}

nvigi::Result FIGIModule::LoadIGIFeature(const nvigi::PluginID& Feature, nvigi::InferenceInterface** Interface, const UTF8CHAR* UTF8PathToPlugin)
{
    const nvigi::Result Result{ Pimpl->LoadIGIFeature(Feature, Interface, UTF8PathToPlugin) };
    if (Result == nvigi::kResultOk)
    {
        UE_LOG(LogIGISDK, Log, TEXT("IGI feature loaded"));
    }
    else
    {
        UE_LOG(LogIGISDK, Fatal, TEXT("ERROR when loading IGI feature"));
    }
    return Result;
}

nvigi::Result FIGIModule::UnloadIGIFeature(const nvigi::PluginID& Feature, nvigi::InferenceInterface* Interface)
{
    const nvigi::Result Result{ Pimpl->UnloadIGIFeature(Feature, Interface) };
    if (Result == nvigi::kResultOk)
    {
        UE_LOG(LogIGISDK, Log, TEXT("IGI feature unloaded"));
    }
    else
    {
        UE_LOG(LogIGISDK, Fatal, TEXT("ERROR when unloading IGI feature"));
    }
    return Result;
}

nvigi::D3D12Parameters FIGIModule::GetD3D12Parameters() const
{
    return Pimpl->GetD3D12Parameters();
}

nvigi::VulkanParameters FIGIModule::GetVulkanParameters() const
{
    return Pimpl->GetVulkanParameters();
}

const FString FIGIModule::GetModelsPath() const
{
    return Pimpl->GetModelsPath();
}

FIGIGPT* FIGIModule::GetGPT()
{
    return Pimpl->GetGPT(this);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FIGIModule, IGI)
