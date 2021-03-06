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

FILE: Methane/Graphics/Metal/BlitCommandListMT.hh
Metal implementation of the blit command list interface.

******************************************************************************/

#pragma once

#include <Methane/Graphics/BlitCommandList.h>
#include <Methane/Graphics/CommandListBase.h>

#import <Metal/Metal.h>

namespace Methane::Graphics
{

class CommandQueueMT;

class BlitCommandListMT final
    : public CommandListBase
    , public BlitCommandList
{
public:
    BlitCommandListMT(CommandQueueBase& command_queue);

    // CommandList interface
    void PushDebugGroup(const std::string& name) override;
    void PopDebugGroup() override;
    void Commit() override;

    // CommandListBase interface
    void SetResourceBarriers(const ResourceBase::Barriers&) override { }
    void Execute(uint32_t frame_index) override;

    // BlitCommandList interface
    void Reset(const std::string& debug_group = "") override;

    // Object interface
    void SetName(const std::string& label) override;

    id<MTLCommandBuffer>&        GetNativeCommandBuffer() noexcept { return m_mtl_cmd_buffer; }
    id<MTLBlitCommandEncoder>&   GetNativeBlitEncoder() noexcept   { return m_mtl_blit_encoder; }

private:
    void InitializeCommandBuffer();
    
    CommandQueueMT& GetCommandQueueMT() noexcept;

    id<MTLCommandBuffer>        m_mtl_cmd_buffer = nil;
    id<MTLBlitCommandEncoder>   m_mtl_blit_encoder = nil;
};

} // namespace Methane::Graphics
