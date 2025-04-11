// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "CoreMinimal.h"
#include "IGILog.h"

#include "nvigi.h"
#include "nvigi_ai.h"
#include "nvigi_struct.h"

class FIGICore
{
public:
    FIGICore(FString IGICoreLibraryPath);
    virtual ~FIGICore();

    bool IsInitialized() const { return bInitialized; }

    nvigi::Result LoadInterface(const nvigi::PluginID& Feature, const nvigi::UID& InterfaceType, nvigi::InferenceInterface** Interface, const UTF8CHAR* UTF8PathToPlugin = nullptr);
    nvigi::Result UnloadInterface(const nvigi::PluginID& Feature, nvigi::InferenceInterface* Interface);
    nvigi::Result CheckPluginCompatibility(const nvigi::PluginID& Feature, const FString& Name);

    static bool IsPhysicalVendor(const nvigi::AdapterSpec* Adapter)
    {
        const bool bIsPhysicalVendor = Adapter->vendor != nvigi::VendorId::eAny || Adapter->vendor != nvigi::VendorId::eNone;
        UE_LOG(LogIGISDK, Log, TEXT("IGI: %s adapter vendor: 0x%X id"), bIsPhysicalVendor ? TEXT("Physical") : TEXT("Not physical"), Adapter->vendor);
        
        return bIsPhysicalVendor;
    }

private:
    void* IGICoreLibraryHandle;

    PFun_nvigiInit* Ptr_nvigiInit{};
    PFun_nvigiShutdown* Ptr_nvigiShutdown{};
    PFun_nvigiLoadInterface* Ptr_nvigiLoadInterface{};
    PFun_nvigiUnloadInterface* Ptr_nvigiUnloadInterface{};

    nvigi::PluginAndSystemInformation* IGIRequirements{};

    FString ModelDirectory;

    bool bInitialized{ false };

    int AdapterId = -1;
};
