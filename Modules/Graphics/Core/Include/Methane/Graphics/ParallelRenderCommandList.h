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

FILE: Methane/Graphics/ParallelRenderCommandList.h
Methane parallel render command list interface
for multi-threaded rendering in the single render pass.

******************************************************************************/

#pragma once

#include "RenderCommandList.h"

namespace Methane::Graphics
{

struct RenderState;
struct RenderPass;

struct ParallelRenderCommandList : virtual CommandList
{
    using Ptr = std::shared_ptr<ParallelRenderCommandList>;

    // Create ParallelRenderCommandList instance
    static Ptr Create(CommandQueue& command_queue, RenderPass& render_pass);

    // ParallelRenderCommandList interface
    virtual void  Reset() = 0;
    virtual RenderCommandList::Ptrs CreateRenderCommandLists(uint32_t count) = 0;
};

} // namespace Methane::Graphics