﻿/******************************************************************************

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

FILE: Methane/Graphics/DirectX12/TextureDX.cpp
DirectX 12 implementation of the texture interface.

******************************************************************************/

#include "TextureDX.h"
#include "ContextDX.h"
#include "DeviceDX.h"
#include "DescriptorHeapDX.h"
#include "CommandQueueDX.h"
#include "RenderCommandListDX.h"
#include "TypesDX.h"

#include <Methane/Instrumentation.h>
#include <Methane/Graphics/Windows/Helpers.h>

namespace Methane::Graphics
{

D3D12_SRV_DIMENSION GetSrvDimension(const Dimensions& tex_dimensions) noexcept
{
    ITT_FUNCTION_TASK();
    return tex_dimensions.depth == 1 ? (tex_dimensions.height == 1 ? D3D12_SRV_DIMENSION_TEXTURE1D
                                                                   : D3D12_SRV_DIMENSION_TEXTURE2D)
                                     : D3D12_SRV_DIMENSION_TEXTURE3D;
}

D3D12_DSV_DIMENSION GetDsvDimension(const Dimensions& tex_dimensions)
{
    ITT_FUNCTION_TASK();
    if (tex_dimensions.depth != 1)
    {
        throw std::runtime_error("Depth-stencil view can not be created for 3D texture.");
    }
    return  tex_dimensions.height == 1 ? D3D12_DSV_DIMENSION_TEXTURE1D
                                       : D3D12_DSV_DIMENSION_TEXTURE2D;
}

Texture::Ptr Texture::CreateRenderTarget(Context& context, const Settings& settings, const DescriptorByUsage& descriptor_by_usage)
{
    ITT_FUNCTION_TASK();
    
    switch (settings.type)
    {
    case Texture::Type::FrameBuffer:        throw std::logic_error("Frame buffer texture must be created with static method Texture::CreateFrameBuffer.");
    case Texture::Type::DepthStencilBuffer: return std::make_shared<DepthStencilBufferTextureDX>(static_cast<ContextBase&>(context), settings, descriptor_by_usage, context.GetSettings().clear_depth, context.GetSettings().clear_stencil);
    default:                                return std::make_shared<RenderTargetTextureDX>(static_cast<ContextBase&>(context), settings, descriptor_by_usage);
    }
}

Texture::Ptr Texture::CreateFrameBuffer(Context& context, uint32_t frame_buffer_index, const DescriptorByUsage& descriptor_by_usage)
{
    ITT_FUNCTION_TASK();
    
    const Context::Settings& context_settings = context.GetSettings();
    const Settings texture_settings = Settings::FrameBuffer(context_settings.frame_size, context_settings.color_format);
    return std::make_shared<FrameBufferTextureDX>(static_cast<ContextBase&>(context), texture_settings, descriptor_by_usage, frame_buffer_index);
}

Texture::Ptr Texture::CreateDepthStencilBuffer(Context& context, const DescriptorByUsage& descriptor_by_usage)
{
    ITT_FUNCTION_TASK();
    
    const Context::Settings& context_settings = context.GetSettings();
    const Settings texture_settings = Settings::DepthStencilBuffer(context_settings.frame_size, context_settings.depth_stencil_format);
    return std::make_shared<DepthStencilBufferTextureDX>(static_cast<ContextBase&>(context), texture_settings, descriptor_by_usage, context_settings.clear_depth, context_settings.clear_stencil);
}

Texture::Ptr Texture::CreateImage(Context& context, Dimensions dimensions, uint32_t array_length, PixelFormat pixel_format, bool mipmapped, const DescriptorByUsage& descriptor_by_usage)
{
    ITT_FUNCTION_TASK();
    const Settings texture_settings = Settings::Image(dimensions, array_length, pixel_format, mipmapped, Usage::ShaderRead);
    return std::make_shared<ImageTextureDX>(static_cast<ContextBase&>(context), texture_settings, descriptor_by_usage, ImageTextureArg());
}

Texture::Ptr Texture::CreateCube(Context& context, uint32_t dimension_size, uint32_t array_length, PixelFormat pixel_format, bool mipmapped, const DescriptorByUsage& descriptor_by_usage)
{
    ITT_FUNCTION_TASK();
    const Settings texture_settings = Settings::Cube(dimension_size, array_length, pixel_format, mipmapped, Usage::ShaderRead);
    return std::make_shared<ImageTextureDX>(static_cast<ContextBase&>(context), texture_settings, descriptor_by_usage, ImageTextureArg());
}

template<>
void RenderTargetTextureDX::Initialize()
{
    ITT_FUNCTION_TASK();

    if (m_settings.dimensions.depth != 1 || m_settings.dimensions.width == 0 || m_settings.dimensions.height == 0)
    {
        throw std::invalid_argument("Render target texture can only be created for 2D texture with dimensions.depth == 1 and non zero width and hight.");
    }

    D3D12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(
        TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format),
        m_settings.dimensions.width, m_settings.dimensions.height
    );
    InitializeCommittedResource(tex_desc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_GENERIC_READ);
}

template<>
void FrameBufferTextureDX::Initialize(uint32_t frame_buffer_index)
{
    ITT_FUNCTION_TASK();

    InitializeFrameBufferResource(frame_buffer_index);

    if (m_usage_mask != Usage::RenderTarget)
    {
        throw std::runtime_error("Frame BufferBase texture supports only Render Target usage.");
    }

    assert(!!m_cp_resource);
    const D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = GetNativeCPUDescriptorHandle(Usage::RenderTarget);
    GetContextDX().GetDeviceDX().GetNativeDevice()->CreateRenderTargetView(m_cp_resource.Get(), nullptr, descriptor_handle);
}

template<>
void DepthStencilBufferTextureDX::Initialize(Depth depth_clear_value, Stencil stencil_clear_value)
{
    ITT_FUNCTION_TASK();

    CD3DX12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(
        TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format, TypeConverterDX::ResourceFormatType::ResourceBase),
        m_settings.dimensions.width, m_settings.dimensions.height
    );

    tex_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_settings.usage_mask & Usage::RenderTarget)
    {
        tex_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    if (!(m_settings.usage_mask & Usage::ShaderRead ||
          m_settings.usage_mask & Usage::ShaderWrite))
    {
        tex_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    // Performance tip: Tell the runtime at resource creation the desired clear value
    const DXGI_FORMAT view_write_format = TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format, TypeConverterDX::ResourceFormatType::ViewWrite);
    CD3DX12_CLEAR_VALUE clear_value(view_write_format, depth_clear_value, stencil_clear_value);
    InitializeCommittedResource(tex_desc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value);

    const wrl::ComPtr<ID3D12Device>& cp_device = GetContextDX().GetDeviceDX().GetNativeDevice();

    for (Usage::Value usage : Usage::values)
    {
        if (!(m_usage_mask & usage))
            continue;

        const Descriptor& desc = ResourceBase::GetDescriptorByUsage(usage);
        switch (desc.heap.GetSettings().type)
        {
        case DescriptorHeap::Type::ShaderResources:
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format                          = TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format, TypeConverterDX::ResourceFormatType::ViewRead);
            srv_desc.ViewDimension                   = GetSrvDimension(m_settings.dimensions);
            srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Texture2D.MipLevels             = 1;
            cp_device->CreateShaderResourceView(m_cp_resource.Get(), &srv_desc, GetNativeCPUDescriptorHandle(desc));
        } break;

        case DescriptorHeap::Type::DepthStencil:
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc   = {};
            dsv_desc.Format                          = view_write_format;
            dsv_desc.ViewDimension                   = GetDsvDimension(m_settings.dimensions);
            cp_device->CreateDepthStencilView(m_cp_resource.Get(), &dsv_desc, GetNativeCPUDescriptorHandle(desc));
        } break;

        default:
            throw std::runtime_error("Unsupported usage of Depth-Stencil buffer.");
        }
    }
}

ImageTextureDX::TextureDX(ContextBase& context, const Settings& settings, const DescriptorByUsage& descriptor_by_usage, ImageTextureArg)
    : TextureBase(context, settings, descriptor_by_usage)
{
    ITT_FUNCTION_TASK();

    if (m_usage_mask != Usage::ShaderRead)
    {
        throw std::runtime_error("Image texture supports only \"Shader Read\" usage.");
    }

    InitializeDefaultDescriptors();

    assert(m_settings.dimensions.depth > 0);
    assert(m_settings.dimensions.width > 0);
    assert(m_settings.dimensions.height > 0);

    D3D12_RESOURCE_DESC tex_desc = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};

    switch(m_settings.dimension_type)
    {
    case DimensionType::Tex1D:
        if (m_settings.array_length != 1)
        {
            throw std::invalid_argument("Single 1D Texture must have array length equal to 1.");
        }
        srv_desc.Texture1D.MipLevels = 1; // TODO: set actual mip-levels count
    case DimensionType::Tex1DArray:
        if (m_settings.dimensions.height != 1 || m_settings.dimensions.depth != 1)
        {
            throw std::invalid_argument("1D Textures must have height and depth dimensions equal to 1.");
        }

        tex_desc = CD3DX12_RESOURCE_DESC::Tex1D(
            TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format),
            m_settings.dimensions.width,
            m_settings.array_length,
            m_settings.mipmapped ? 0 : 1
        );

        srv_desc.Texture1DArray.MipLevels = 1; // TODO: set actual mip-levels count
        srv_desc.ViewDimension            = m_settings.dimension_type == DimensionType::Tex1D
                                          ? D3D12_SRV_DIMENSION_TEXTURE1D
                                          : D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
        break;

    case DimensionType::Tex2D:
        if (m_settings.array_length != 1)
        {
            throw std::invalid_argument("Single 2D Texture must have array length equal to 1.");
        }
        srv_desc.Texture2D.MipLevels = 1; // TODO: set actual mip-levels count
    case DimensionType::Tex2DArray:
        if (m_settings.dimensions.depth != 1)
        {
            throw std::invalid_argument("2D Textures must have depth dimension equal to 1.");
        }
        tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(
            TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format),
            m_settings.dimensions.width,
            m_settings.dimensions.height,
            m_settings.array_length,
            m_settings.mipmapped ? 0 : 1
        );

        srv_desc.Texture2DArray.MipLevels = 1; // TODO: set actual mip-levels count
        srv_desc.ViewDimension            = m_settings.dimension_type == DimensionType::Tex2D
                                          ? D3D12_SRV_DIMENSION_TEXTURE2D
                                          : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        break;

    case DimensionType::Tex3D:
        if (m_settings.array_length != 1)
        {
            throw std::invalid_argument("Single 3D Texture must have array length equal to 1.");
        }
        tex_desc = CD3DX12_RESOURCE_DESC::Tex3D(
            TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format),
            m_settings.dimensions.width,
            m_settings.dimensions.height,
            m_settings.dimensions.depth,
            m_settings.mipmapped ? 0 : 1
        );
        srv_desc.Texture3D.MipLevels = 1; // TODO: set actual mip-levels count
        srv_desc.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE3D;
        break;

    case DimensionType::Cube:
        if (m_settings.array_length != 1)
        {
            throw std::invalid_argument("Single Cube Texture must have array length equal to 1.");
        }
        srv_desc.TextureCube.MipLevels = 1; // TODO: set actual mip-levels count
    case DimensionType::CubeArray:
        if (m_settings.dimensions.depth != 6)
        {
            throw std::invalid_argument("Cube textures must have depth dimension equal to 6.");
        }
        tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(
            TypeConverterDX::DataFormatToDXGI(m_settings.pixel_format),
            m_settings.dimensions.width,
            m_settings.dimensions.height,
            m_settings.dimensions.depth * m_settings.array_length,
            m_settings.mipmapped ? 0 : 1
        );

        srv_desc.TextureCubeArray.First2DArrayFace = 0;
        srv_desc.TextureCubeArray.NumCubes = m_settings.array_length;
        srv_desc.TextureCubeArray.MipLevels = 1; // TODO: set actual mip-levels count
        srv_desc.ViewDimension              = m_settings.dimension_type == DimensionType::Cube
                                            ? D3D12_SRV_DIMENSION_TEXTURECUBE
                                            : D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        break;
    }

    srv_desc.Format = tex_desc.Format;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    InitializeCommittedResource(tex_desc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);

    const UINT number_of_subresources = m_settings.dimensions.depth * m_settings.array_length;
    const UINT64 upload_buffer_size   = GetRequiredIntermediateSize(m_cp_resource.Get(), 0, number_of_subresources);
    ThrowIfFailed(
        GetContextDX().GetDeviceDX().GetNativeDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_cp_upload_resource)
        )
    );

    GetContextDX().GetDeviceDX().GetNativeDevice()->CreateShaderResourceView(m_cp_resource.Get(), &srv_desc, GetNativeCPUDescriptorHandle(Usage::ShaderRead));
}

void ImageTextureDX::SetData(const SubResources& sub_resources)
{
    ITT_FUNCTION_TASK();

    if (sub_resources.empty())
    {
        throw std::invalid_argument("Can not set texture data from empty sub-resources.");
    }
    
    assert(!!m_cp_resource);
    assert(!!m_cp_upload_resource);

    const uint32_t      mip_levels_count  = 1; // TODO: replace with actual value when mip-levels are supported
    const Data::Size    pixel_size = GetPixelSize(m_settings.pixel_format);
    const uint32_t first_sub_resource_index = sub_resources.front().GetIndex(mip_levels_count);

    m_data_size = 0;
    std::vector<D3D12_SUBRESOURCE_DATA> dx_sub_resources(sub_resources.size(), D3D12_SUBRESOURCE_DATA{});

    for(const SubResource& sub_resourse : sub_resources)
    {
        const uint32_t raw_sub_resource_index = sub_resourse.GetIndex(m_settings.dimensions.depth, mip_levels_count) - first_sub_resource_index;
        if (raw_sub_resource_index >= dx_sub_resources.size())
        {
            dx_sub_resources.resize(raw_sub_resource_index + 1, D3D12_SUBRESOURCE_DATA{});
        }

        D3D12_SUBRESOURCE_DATA& dx_sub_resource = dx_sub_resources[raw_sub_resource_index];
        dx_sub_resource.pData      = sub_resourse.p_data;
        dx_sub_resource.RowPitch   = m_settings.dimensions.width  * pixel_size;
        dx_sub_resource.SlicePitch = m_settings.dimensions.height * dx_sub_resource.RowPitch;

        if (dx_sub_resource.SlicePitch > static_cast<LONG_PTR>(sub_resourse.data_size))
        {
            throw std::invalid_argument("Sub-resource data size is smaller than computed slice size. Possible pixel format mismatch.");
        }

        m_data_size += static_cast<Data::Size>(dx_sub_resource.SlicePitch);
    }

    RenderCommandListDX& upload_cmd_list = static_cast<RenderCommandListDX&>(m_context.GetUploadCommandList());
    UpdateSubresources(upload_cmd_list.GetNativeCommandList().Get(),
                       m_cp_resource.Get(), m_cp_upload_resource.Get(), 0, first_sub_resource_index,
                       static_cast<UINT>(dx_sub_resources.size()), dx_sub_resources.data());

    upload_cmd_list.SetResourceTransitionBarriers({ static_cast<Resource&>(*this) }, ResourceBase::State::CopyDest, ResourceBase::State::PixelShaderResource);
}

} // namespace Methane::Graphics
