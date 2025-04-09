// SPDX-FileCopyrightText: Copyright (c) SPACE KIWI STUDIO. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#define IGI_USE_GRAPHICS_API_D3D12		1
#define IGI_USE_GRAPHICS_API_VULKAN		1
#endif

#if PLATFORM_LINUX
#define IGI_USE_GRAPHICS_API_D3D12		0
#define IGI_USE_GRAPHICS_API_VULKAN		1
#endif


//-------------------------------------------------------------------------------------------------
// D3D12
//-------------------------------------------------------------------------------------------------
#ifdef IGI_USE_GRAPHICS_API_D3D12
#include "ID3D12DynamicRHI.h"
#pragma warning( push )
#pragma warning( disable : 5257 )
#include "nvigi_d3d12.h"
#pragma warning( pop )
#endif // IGI_USE_GRAPHICS_API_D3D12

//-------------------------------------------------------------------------------------------------
// Vulkan
//-------------------------------------------------------------------------------------------------
#ifdef IGI_USE_GRAPHICS_API_VULKAN
#include "IVulkanDynamicRHI.h"
#include "nvigi_vulkan.h"
#endif // IGI_USE_GRAPHICS_API_VULKAN