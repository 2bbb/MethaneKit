/******************************************************************************

Copyright 2019-2020 Evgeny Gorodetskiy

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*******************************************************************************

FILE: Methane/Graphics/DirectX12/RenderContextDX.cpp
DirectX 12 implementation of the render context interface.

******************************************************************************/

#pragma once

#include "ContextDX.hpp"

#include <Methane/Graphics/RenderContextBase.h>

#include <dxgi1_4.h>
#include <vector>

namespace Methane::Graphics
{

class RenderContextDX final : public ContextDX<RenderContextBase>
{
public:
    RenderContextDX(const Platform::AppEnvironment& env, DeviceBase& device, const RenderContext::Settings& settings);

    ~RenderContextDX() override;

    // RenderContext interface
    bool ReadyToRender() const override { return true; }
    void WaitForGpu(WaitFor wait_for) override;
    void Resize(const FrameSize& frame_size) override;
    void Present() override;
    Platform::AppView GetAppView() const override { return { nullptr }; }
    float GetContentScalingFactor() const override;

    // ContextBase interface
    void Initialize(DeviceBase& device, bool deferred_heap_allocation) override;
    void Release() override;

    // Object interface
    void SetName(const std::string& name) override;

    CommandQueueDX& GetRenderCommandQueueDX();

    const wrl::ComPtr<IDXGISwapChain3>& GetNativeSwapChain() const { return m_cp_swap_chain; }

protected:
    FrameFenceDX&                         GetCurrentFrameFence();
    inline const UniquePtr<FrameFenceDX>& GetCurrentFrameFencePtr()       { return m_frame_fences[m_frame_buffer_index]; }
    inline uint32_t                       GetPresentVSyncInterval() const { return m_settings.vsync_enabled ? 1 : 0; }

    const Platform::AppEnvironment m_platform_env;
    wrl::ComPtr<IDXGISwapChain3>   m_cp_swap_chain;
    UniquePtrs<FrameFenceDX>       m_frame_fences;
    Ptr<FenceDX>                   m_sp_render_fence;
};

} // namespace Methane::Graphics