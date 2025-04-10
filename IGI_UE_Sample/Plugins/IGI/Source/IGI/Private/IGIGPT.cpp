// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "IGIGPT.h"

#include "CoreMinimal.h"
#include "IGIPlatformRHI.h"
#include "IGIMinimal.h"

#include "nvigi_gpt.h"

#include <condition_variable>
#include <thread>
#include <mutex>

namespace
{
    constexpr const char* const GGUF_MODEL_MINITRON{ "{01F43B70-CE23-42CA-9606-74E80C5ED0B6}" };

    constexpr std::size_t VRAM_BUDGET_RECOMMENDATION{ 1024 * 12 };
    constexpr std::size_t THREAD_NUM_RECOMMENDATION{ 1 }; // Recommended number of threads for CiG
    constexpr std::size_t CONTEXT_SIZE_RECOMMENDATION{ 4096 };
}

class FIGIGPT::Impl
{
public:
    Impl(FIGIModule* IGIModule)
        : IGIModulePtr(IGIModule)
    {
        nvigi::Result Result = nvigi::kResultOk;

        IGIModulePtr->LoadIGIFeature(nvigi::plugin::gpt::ggml::cuda::kId, &GPTInterface, nullptr);

        nvigi::GPTCreationParameters params{};

        nvigi::CommonCreationParameters common{};
        auto ConvertedString = StringCast<UTF8CHAR>(*IGIModulePtr->GetModelsPath());
        common.utf8PathToModels = reinterpret_cast<const char*>(ConvertedString.Get());
        common.numThreads = THREAD_NUM_RECOMMENDATION;
        common.vramBudgetMB = VRAM_BUDGET_RECOMMENDATION;
        common.modelGUID = GGUF_MODEL_MINITRON;
        Result = params.chain(common);
        if (Result != nvigi::kResultOk)
        {
            UE_LOG(LogIGISDK, Error, TEXT("Unable to chain common parameters; cannot use CiG: %s"), *GetIGIStatusString(Result));
            GPTInstance = nullptr;
            return;
        }
        
        if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12)
        {
            Result = params.chain(IGIModulePtr->GetD3D12Parameters());
            if (Result != nvigi::kResultOk)
            {
                UE_LOG(LogIGISDK, Error, TEXT("Unable to chain D3D12 parameters; cannot use CiG: %s"), *GetIGIStatusString(Result));
                GPTInstance = nullptr;
                return;
            }
        }
        else if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Vulkan)
        {
            Result = params.chain(IGIModulePtr->GetVulkanParameters());
            if (Result != nvigi::kResultOk)
            {
                UE_LOG(LogIGISDK, Error, TEXT("Unable to chain Vulkan parameters; cannot use CiG: %s"), *GetIGIStatusString(Result));
                GPTInstance = nullptr;
                return;
            }
        }

        Result = GPTInterface->createInstance(params, &GPTInstance);
        if (Result != nvigi::kResultOk)
        {
            UE_LOG(LogIGISDK, Fatal, TEXT("Unable to create gpt.ggml.cuda instance: %s"), *GetIGIStatusString(Result));
            GPTInstance = nullptr;
            return;
        }
    }

    virtual ~Impl()
    {
        if (GPTInstance != nullptr)
        {
            GPTInterface->destroyInstance(GPTInstance);
            GPTInstance = nullptr;
        }

        if (IGIModulePtr)
        {
            IGIModulePtr->UnloadIGIFeature(nvigi::plugin::gpt::ggml::cuda::kId, GPTInterface);
            IGIModulePtr = nullptr;
        }
    }

    FString Evaluate(const FString& SystemPrompt, const FString& UserPrompt, const FString& AssistantPrompt)
    {
        FScopeLock Lock(&CS);

        struct BasicCallbackCtx
        {
            std::mutex callbackMutex;
            std::condition_variable callbackCV;
            std::atomic<nvigi::InferenceExecutionState> callbackState = nvigi::kInferenceExecutionStateDataPending;
            FString gptOutput;
        };
        BasicCallbackCtx cbkCtx;

        auto completionCallback = [](const nvigi::InferenceExecutionContext* ctx, nvigi::InferenceExecutionState state, void* data) -> nvigi::InferenceExecutionState
            {
                if (!data)
                    return nvigi::kInferenceExecutionStateInvalid;

                auto cbkCtx = (BasicCallbackCtx*)data;
                std::scoped_lock lck(cbkCtx->callbackMutex);

                // Outputs from GPT
                auto slots = ctx->outputs;
                const nvigi::InferenceDataText* text{};
                slots->findAndValidateSlot(nvigi::kGPTDataSlotResponse, &text);
                auto response = FString(StringCast<UTF8CHAR>(text->getUTF8Text()));
                if (response.Find("<JSON>") != INDEX_NONE)
                {
                    auto cpuBuffer = castTo<nvigi::CpuData>(text->utf8Text);
                    ((uint8_t*)cpuBuffer->buffer)[0] = 0;
                    cpuBuffer->sizeInBytes = 0;
                }
                else
                {
                    cbkCtx->gptOutput += response;
                }

                cbkCtx->callbackState = state;
                cbkCtx->callbackCV.notify_one();

                return state;
            };

        auto SystemPromptUTF = StringCast<UTF8CHAR>(*SystemPrompt);
        nvigi::InferenceDataTextSTLHelper SystemPromptData(reinterpret_cast<const char*>(SystemPromptUTF.Get()));

        auto UserPromptUTF = StringCast<UTF8CHAR>(*UserPrompt);
        nvigi::InferenceDataTextSTLHelper UserPromptData(reinterpret_cast<const char*>(UserPromptUTF.Get()));

        auto AssistantPromptUTF = StringCast<UTF8CHAR>(*AssistantPrompt);
        nvigi::InferenceDataTextSTLHelper AssistantPromptData(reinterpret_cast<const char*>(AssistantPromptUTF.Get()));

        TArray<nvigi::InferenceDataSlot> inSlots = {
            {nvigi::kGPTDataSlotUser, UserPromptData}
        };
        if (SystemPrompt.Len() > 0u)
        {
            inSlots.Add({ nvigi::kGPTDataSlotSystem, SystemPromptData });
        }
        if (AssistantPrompt.Len() > 0u)
        {
            inSlots.Add({ nvigi::kGPTDataSlotAssistant, AssistantPromptData });
        }

        nvigi::InferenceDataSlotArray inputs = { static_cast<size_t>(inSlots.Num()), inSlots.GetData() };

        // Parameters
        nvigi::GPTRuntimeParameters runtime{};
        runtime.seed = -1;
        runtime.tokensToPredict = 200;
        runtime.interactive = false;

        nvigi::InferenceExecutionContext gptCtx{};
        nvigi::InferenceInstance* instance = GPTInstance;
        gptCtx.instance = instance;
        gptCtx.callback = completionCallback;
        gptCtx.callbackUserData = &cbkCtx;
        gptCtx.inputs = &inputs;
        gptCtx.runtimeParameters = runtime;

        cbkCtx.callbackState = nvigi::kInferenceExecutionStateDataPending;

        instance->evaluateAsync(&gptCtx);

        {
            std::unique_lock lck(cbkCtx.callbackMutex);
            cbkCtx.callbackCV.wait(lck, [&cbkCtx]()
                {
                    return cbkCtx.callbackState != nvigi::kInferenceExecutionStateDataPending;
                });
        }

        FString response(cbkCtx.gptOutput);

        return response;
    }

private:
    FCriticalSection CS;

    // Non-owning ptr
    FIGIModule* IGIModulePtr;

    nvigi::IGeneralPurposeTransformer* GPTInterface{ nullptr };
    nvigi::InferenceInstance* GPTInstance{ nullptr };
};

// ----------------------------------

FIGIGPT::FIGIGPT(FIGIModule* IGIModule)
{
    Pimpl = MakePimpl<FIGIGPT::Impl>(IGIModule);
}

FIGIGPT::~FIGIGPT() {}

FString FIGIGPT::Evaluate(const FString& SystemPrompt, const FString& UserPrompt, const FString& AssistantPrompt)
{
    return Pimpl->Evaluate(SystemPrompt, UserPrompt, AssistantPrompt);
}
