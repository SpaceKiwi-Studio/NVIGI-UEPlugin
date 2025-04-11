// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "IGICore.h"

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"

#include "IGILog.h"

#include "nvigi.h"
#include "nvigi_ai.h"
#include "nvigi_struct.h"

#if PLATFORM_WINDOWS
// nvigi_types.h requires windows headers for LUID definition, only on Windows.
#include "Windows/AllowWindowsPlatformTypes.h"
#include "nvigi_types.h"
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include "nvigi_types.h"
#endif

FIGICore::FIGICore(FString IGICoreLibraryPath)
{
    IGICoreLibraryHandle = !IGICoreLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*IGICoreLibraryPath) : nullptr;

    if (IGICoreLibraryHandle)
    {
        Ptr_nvigiInit = (PFun_nvigiInit*)FPlatformProcess::GetDllExport(IGICoreLibraryHandle, TEXT("nvigiInit"));
        Ptr_nvigiShutdown = (PFun_nvigiShutdown*)FPlatformProcess::GetDllExport(IGICoreLibraryHandle, TEXT("nvigiShutdown"));
        Ptr_nvigiLoadInterface = (PFun_nvigiLoadInterface*)FPlatformProcess::GetDllExport(IGICoreLibraryHandle, TEXT("nvigiLoadInterface"));
        Ptr_nvigiUnloadInterface = (PFun_nvigiUnloadInterface*)FPlatformProcess::GetDllExport(IGICoreLibraryHandle, TEXT("nvigiUnloadInterface"));
    }
    else
    {
        UE_LOG(LogIGISDK, Fatal, TEXT("IGI: Failed to load IGI core library... Aborting."));
        bInitialized = false;
        return;
    }

    if (!(Ptr_nvigiInit && Ptr_nvigiShutdown && Ptr_nvigiLoadInterface && Ptr_nvigiUnloadInterface))
    {
        UE_LOG(LogIGISDK, Fatal, TEXT("IGI: Failed to load IGI core library functions... Aborting."));
        bInitialized = false;
        return;
    }

    nvigi::Preferences Pref{};
#if UE_BUILD_SHIPPING
    Pref.showConsole = false;
#else
    Pref.showConsole = true;
#endif
    Pref.logLevel = nvigi::LogLevel::eDefault;

    const FString BaseDir = IPluginManager::Get().FindPlugin("IGI")->GetBaseDir();
    const FString IGIPluginPath = FPaths::Combine(*BaseDir, TEXT("ThirdParty/nvigi_pack/plugins/sdk/bin/x64"));
    const auto IGIPluginPathUTF8 = StringCast<UTF8CHAR>(*IGIPluginPath);
    const char* IGIPluginPathCStr = reinterpret_cast<const char*>(IGIPluginPathUTF8.Get());
    Pref.utf8PathsToPlugins = &IGIPluginPathCStr;
    Pref.numPathsToPlugins = 1u;

    const auto IGILogsPathUTF8 = StringCast<UTF8CHAR>(*FPaths::ProjectLogDir());
    const char* IGILogsPathCStr = reinterpret_cast<const char*>(IGILogsPathUTF8.Get());
    Pref.utf8PathToLogsAndData = IGILogsPathCStr;

    Pref.logMessageCallback = IGILogCallback;

    nvigi::Result InitResult = (*Ptr_nvigiInit)(Pref, &IGIRequirements, nvigi::kSDKVersion);

    // Find HW Adapter
    uint32_t HWAdapter = 0;
    for (int i = 0; i < IGIRequirements->numDetectedAdapters; i++)
    {
        const auto& Adapter = IGIRequirements->detectedAdapters[i];
        if (IsPhysicalVendor(Adapter) && HWAdapter < Adapter->architecture)
        {
            UE_LOG(LogIGISDK, Log, TEXT("IGI: Found adapter %d: vendor: 0x%X ; architecture: %u"), i, Adapter->vendor, Adapter->architecture);
            HWAdapter = Adapter->architecture;
            AdapterId = i;
        }
    }

    if (AdapterId < 0)
    {
        UE_LOG(LogIGISDK, Warning, TEXT("No hardware adapters found.  GPU plugins will not be available"));
        if (IGIRequirements->numDetectedAdapters)
            AdapterId = 0;
    }
    
    UE_LOG(LogIGISDK, Log, TEXT("IGI: Init result: %u"), InitResult);

    bInitialized = true;
}

FIGICore::~FIGICore()
{
    // Free the dll handle
    FPlatformProcess::FreeDllHandle(IGICoreLibraryHandle);
    IGICoreLibraryHandle = nullptr;
}

nvigi::Result FIGICore::LoadInterface(const nvigi::PluginID& Feature, const nvigi::UID& InterfaceType, nvigi::InferenceInterface** Interface, const UTF8CHAR* UTF8PathToPlugin)
{
    if (Ptr_nvigiLoadInterface == nullptr)
    {
        return nvigi::kResultInvalidState;
    }

    nvigi::InferenceInterface DummyInterface;

    nvigi::Result Result = (*Ptr_nvigiLoadInterface)(Feature, InterfaceType, DummyInterface.getVersion(), reinterpret_cast<void**>(Interface), BitCast<const char*>(UTF8PathToPlugin));
    UE_LOG(LogIGISDK, Log, TEXT("IGI: LoadInterface result: %u"), Result);

    return Result;
}

nvigi::Result FIGICore::UnloadInterface(const nvigi::PluginID& Feature, nvigi::InferenceInterface* Interface)
{
    if (Ptr_nvigiUnloadInterface == nullptr)
    {
        return nvigi::kResultInvalidState;
    }

    nvigi::Result Result = (*Ptr_nvigiUnloadInterface)(Feature, Interface);
    UE_LOG(LogIGISDK, Log, TEXT("IGI: UnloadInterface result: %u"), Result);

    return Result;
}

nvigi::Result FIGICore::CheckPluginCompatibility(const nvigi::PluginID& Feature, const FString& Name)
{
    const nvigi::AdapterSpec* AdapterInfo = (AdapterId >= 0) ? IGIRequirements->detectedAdapters[AdapterId] : nullptr;

    for (int i = 0; i < IGIRequirements->numDetectedPlugins; ++i)
    {
        const auto& Plugin = IGIRequirements->detectedPlugins[i];
        
        if (Plugin->id == Feature)
        {
            if (Plugin->requiredAdapterVendor != nvigi::VendorId::eAny && Plugin->requiredAdapterVendor != nvigi::VendorId::eNone && 
                (!AdapterInfo || Plugin->requiredAdapterVendor != AdapterInfo->vendor))
            {
                UE_LOG(LogIGISDK, Warning, TEXT("Plugin %s could not be loaded on adapters from this GPU vendor (found %0x, requires %0x)"), *Name,
                    AdapterInfo->vendor, Plugin->requiredAdapterVendor);
                return nvigi::kResultInvalidState;
            }

            if (Plugin->requiredAdapterVendor == nvigi::VendorId::eNVDA && Plugin->requiredAdapterArchitecture > AdapterInfo->architecture)
            {
                UE_LOG(LogIGISDK, Warning, TEXT("Plugin %s could not be loaded on this GPU architecture (found %d, requires %d)"), *Name,
                    AdapterInfo->architecture, Plugin->requiredAdapterArchitecture);
                return nvigi::kResultNoSupportedHardwareFound;
            }

            if (Plugin->requiredAdapterVendor == nvigi::VendorId::eNVDA && Plugin->requiredAdapterDriverVersion > AdapterInfo->driverVersion)
            {
                UE_LOG(LogIGISDK, Warning, TEXT("Plugin %s could not be loaded on this driver (found %d.%d, requires %d.%d)"), *Name,
                    AdapterInfo->driverVersion.major, AdapterInfo->driverVersion.minor,
                    Plugin->requiredAdapterDriverVersion.major, Plugin->requiredAdapterDriverVersion.minor);
                return nvigi::kResultDriverOutOfDate;
            }

            return nvigi::kResultOk;
        }
    }
    
    // Not found
    UE_LOG(LogIGISDK, Warning, TEXT("Plugin %s could not be loaded"), *Name);

    return nvigi::kResultNoPluginsFound;
}
