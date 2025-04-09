// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "CoreMinimal.h"

#include "nvigi.h"

DEFINE_LOG_CATEGORY_STATIC(LogIGISDK, Log, All);

static void IGILogCallback(nvigi::LogType Type, const char* InMessage)
{
    FString Message(reinterpret_cast<const UTF8CHAR*>(InMessage));

    // AIM log messages end with newlines, so fix that
    Message.TrimEndInline();
    
    if (Type == nvigi::LogType::eInfo)
    {
        UE_LOG(LogIGISDK, Log, TEXT("IGI: %s"), *Message);
    }
    else if (Type == nvigi::LogType::eWarn)
    {
        UE_LOG(LogIGISDK, Warning, TEXT("IGI: %s"), *Message);
    }
    else if (Type == nvigi::LogType::eError)
    {
        UE_LOG(LogIGISDK, Error, TEXT("IGI: %s"), *Message);
    }
    else
    {
        UE_LOG(LogIGISDK, Error, TEXT("Received unknown IGI log type %d: %s"), Type, *Message);
    }
}

static FString GetIGIStatusString(nvigi::Result Result)
{
    switch (Result)
    {
    case nvigi::kResultOk:
        return FString(TEXT("Success"));
    case nvigi::kResultDriverOutOfDate:
        return FString(TEXT("Driver out of date"));
    case nvigi::kResultOSOutOfDate:
        return FString(TEXT("OS out of date"));
    case nvigi::kResultNoPluginsFound:
        return FString(TEXT("No plugins found"));
    case nvigi::kResultInvalidParameter:
        return FString(TEXT("Invalid parameter"));
    case nvigi::kResultNoSupportedHardwareFound:
        return FString(TEXT("No supported hardware found"));
    case nvigi::kResultMissingInterface:
        return FString(TEXT("Missing interface"));
    case nvigi::kResultMissingDynamicLibraryDependency:
        return FString(TEXT("Missing dynamic library dependency"));
    case nvigi::kResultInvalidState:
        return FString(TEXT("Invalid state"));
    case nvigi::kResultException:
        return FString(TEXT("Exception"));
    case nvigi::kResultJSONException:
        return FString(TEXT("JSON exception"));
    case nvigi::kResultRPCError:
        return FString(TEXT("RPC error"));
    case nvigi::kResultInsufficientResources:
        return FString(TEXT("Insufficient resources"));
    case nvigi::kResultNotReady:
        return FString(TEXT("Not ready"));
    case nvigi::kResultPluginOutOfDate:
        return FString(TEXT("Plugin out of date"));
    case nvigi::kResultDuplicatedPluginId:
        return FString(TEXT("Duplicate plugin ID"));
    case nvigi::kResultNoImplementation:
        return FString(TEXT("No implementation"));
    default:
        return FString(TEXT("invalid IGI error code"));
    }
}
