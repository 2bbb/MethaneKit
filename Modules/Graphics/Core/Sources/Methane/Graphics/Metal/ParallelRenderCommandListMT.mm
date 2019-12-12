/******************************************************************************

Copyright 2019 Evgeny Gorodetskiy

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

FILE: Methane/Graphics/Metal/ParallelRenderCommandListMT.mm
Metal implementation of the render command list interface.

******************************************************************************/

#include "ParallelRenderCommandListMT.hh"
#include "RenderPassMT.hh"
#include "CommandQueueMT.hh"
#include "ContextMT.hh"

#include <Methane/Data/Instrumentation.h>
#include <Methane/Platform/MacOS/Types.hh>

namespace Methane::Graphics
{

ParallelRenderCommandList::Ptr ParallelRenderCommandList::Create(CommandQueue& command_queue, RenderPass& render_pass)
{
    ITT_FUNCTION_TASK();
    return std::make_shared<ParallelRenderCommandListMT>(static_cast<CommandQueueBase&>(command_queue), static_cast<RenderPassBase&>(render_pass));
}

ParallelRenderCommandListMT::ParallelRenderCommandListMT(CommandQueueBase& command_queue, RenderPassBase& render_pass)
    : ParallelRenderCommandListBase(command_queue, render_pass)
    , m_mtl_cmd_buffer(nil)
    , m_mtl_parallel_render_encoder(nil)
{
    ITT_FUNCTION_TASK();
}

void ParallelRenderCommandListMT::SetName(const std::string& name)
{
    ITT_FUNCTION_TASK();

    ParallelRenderCommandListBase::SetName(name);
    
    NSString* ns_name = MacOS::ConvertToNSType<std::string, NSString*>(name);
    
    if (m_mtl_parallel_render_encoder != nil)
    {
        m_mtl_parallel_render_encoder.label = ns_name;
    }
    
    if (m_mtl_cmd_buffer != nil)
    {
        m_mtl_cmd_buffer.label = ns_name;
    }
}

void ParallelRenderCommandListMT::Reset()
{
    ITT_FUNCTION_TASK();
    if (m_mtl_parallel_render_encoder != nil)
        return;

    // NOTE:
    //  If command buffer was not created for current frame yet,
    //  then render pass descriptor should be reset with new frame drawable
    RenderPassMT& render_pass = GetPassMT();
    MTLRenderPassDescriptor* mtl_render_pass = render_pass.GetNativeDescriptor(m_mtl_cmd_buffer == nil);

    if (m_mtl_cmd_buffer == nil)
    {
        m_mtl_cmd_buffer = [GetCommandQueueMT().GetNativeCommandQueue() commandBuffer];
        assert(m_mtl_cmd_buffer != nil);

        m_mtl_cmd_buffer.label = MacOS::ConvertToNSType<std::string, NSString*>(GetName());
    }

    assert(!!mtl_render_pass);
    m_mtl_parallel_render_encoder = [m_mtl_cmd_buffer parallelRenderCommandEncoderWithDescriptor: mtl_render_pass];

    assert(m_mtl_parallel_render_encoder != nil);
    m_mtl_parallel_render_encoder.label = MacOS::ConvertToNSType<std::string, NSString*>(GetName());

    ParallelRenderCommandListBase::Reset();
}

void ParallelRenderCommandListMT::Commit(bool present_drawable)
{
    ITT_FUNCTION_TASK();
    
    assert(!IsCommitted());

    ParallelRenderCommandListBase::Commit(present_drawable);
    
    if (!m_mtl_cmd_buffer || !m_mtl_parallel_render_encoder)
        return;

    [m_mtl_parallel_render_encoder endEncoding];
    m_mtl_parallel_render_encoder = nil;
    
    if (present_drawable)
    {
        [m_mtl_cmd_buffer presentDrawable: GetCommandQueueMT().GetContextMT().GetNativeDrawable()];
    }

    [m_mtl_cmd_buffer enqueue];
}

void ParallelRenderCommandListMT::Execute(uint32_t frame_index)
{
    ITT_FUNCTION_TASK();

    ParallelRenderCommandListBase::Execute(frame_index);

    [m_mtl_cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        Complete(frame_index);
    }];

    [m_mtl_cmd_buffer commit];
    m_mtl_cmd_buffer  = nil;
}

CommandQueueMT& ParallelRenderCommandListMT::GetCommandQueueMT() noexcept
{
    ITT_FUNCTION_TASK();
    return static_cast<class CommandQueueMT&>(*m_sp_command_queue);
}

RenderPassMT& ParallelRenderCommandListMT::GetPassMT()
{
    ITT_FUNCTION_TASK();
    return static_cast<class RenderPassMT&>(GetPass());
}

} // namespace Methane::Graphics