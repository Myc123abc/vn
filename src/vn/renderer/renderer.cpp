#include "renderer.hpp"
#include "core.hpp"
#include "../util.hpp"
#include "message_queue.hpp"

#include <dxcapi.h>

#include <utf8.h>

#include <algorithm>
#include <ranges>

using namespace vn;
using namespace Microsoft::WRL;

////////////////////////////////////////////////////////////////////////////////
///                             Help Functions
////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////
///                             Compiler Shader
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
auto to_wstring(std::string_view str) noexcept -> std::wstring
{
  auto u16str = utf8::utf8to16(str);
  return { reinterpret_cast<wchar_t*>(u16str.data()), u16str.size() };
}
#endif

auto compile_shader(IDxcCompiler3* compiler, IDxcUtils* utils, DxcBuffer& buffer, std::string_view main, std::string_view profile) noexcept
{
  auto args = ComPtr<IDxcCompilerArgs>{};
#ifndef NDEBUG
  auto debug_args = std::vector<LPCWSTR>
  {
    L"-Zi",
    L"-Qembed_debug",
    L"-Od",
  };
  vn::err_if(utils->BuildArguments(nullptr, to_wstring(main).data(), to_wstring(profile).data(), debug_args.data(), debug_args.size(), nullptr, 0, args.GetAddressOf()),
              "failed to create dxc args");
#else
  vn::err_if(utils->BuildArguments(nullptr, to_wstring(main).data(), to_wstring(profile).data(), nullptr, 0, nullptr, 0, args.GetAddressOf()),
              "failed to create dxc args");
#endif
  
  auto result = ComPtr<IDxcResult>{};
  vn::err_if(compiler->Compile(&buffer, args->GetArguments(), args->GetCount(), nullptr, IID_PPV_ARGS(&result)),
              "failed to compile {}", main);
  
  auto hr = HRESULT{};
  result->GetStatus(&hr);
  if (FAILED(hr))
  {
    auto msg = ComPtr<IDxcBlobUtf8>{};
    vn::err_if(result->GetOutput(DXC_OUT_ERRORS,  IID_PPV_ARGS(&msg), nullptr), "failed to get error ouput of dxc");
    vn::err_if(true, "failed to compile {}\n{}", main, msg->GetStringPointer());
  }

  auto cso = ComPtr<IDxcBlob>{};
  vn::err_if(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&cso), nullptr), "failed to get compile result");

  return cso;
}

auto compile_vert_pixel(std::string_view path, std::string_view vert_main, std::string_view pixel_main) noexcept -> std::pair<ComPtr<IDxcBlob>, ComPtr<IDxcBlob>>
{
  auto compiler = ComPtr<IDxcCompiler3>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
              "failed to create dxc compiler");

  DxcBuffer buffer{};
  auto data       = vn::read_file(path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto utils = ComPtr<IDxcUtils>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
              "failed to create dxc utils");
  return { compile_shader(compiler.Get(), utils.Get(), buffer, vert_main, "vs_6_0"), compile_shader(compiler.Get(), utils.Get(), buffer, pixel_main, "ps_6_0") };
}

auto compile_comp(std::string_view path, std::string_view comp_main) noexcept
{
  auto compiler = ComPtr<IDxcCompiler3>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
              "failed to create dxc compiler");

  DxcBuffer buffer{};
  auto data       = vn::read_file(path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto utils = ComPtr<IDxcUtils>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
              "failed to create dxc utils");
  return compile_shader(compiler.Get(), utils.Get(), buffer, comp_main, "cs_6_0");
}

////////////////////////////////////////////////////////////////////////////////
///                             Load Cursor
////////////////////////////////////////////////////////////////////////////////

struct Bitmap
{
  std::vector<std::byte> pixels;
  uint32_t               width{};
  uint32_t               height{};
  uint32_t               pixel_size{};
  glm::vec2              pos{};

  auto data()            noexcept { return pixels.data();               }
  auto byte_size() const noexcept { return width * height * pixel_size; }
  auto row_pitch() const noexcept { return width * pixel_size;          }

  void init(uint32_t width, uint32_t height, uint32_t pixel_size) noexcept
  { 
    this->width      = width;
    this->height     = height;
    this->pixel_size = pixel_size;
    pixels.resize(byte_size());
  }
};

auto load_cursor_bitmap(LPCSTR idc_cursor) noexcept
{
  auto cursor = LoadCursorA(nullptr, idc_cursor);
  err_if(!cursor, "failed to load cursor");

  auto info = ICONINFO{};
  err_if(!GetIconInfo(cursor, &info), "failed to get cursor information");

  auto bitmap = BITMAP{};
  err_if(!GetObjectA(info.hbmColor, sizeof(bitmap), &bitmap), "failed to get bitmap of cursor");

  auto cursor_bitmap = Bitmap{};
  cursor_bitmap.init(bitmap.bmWidth, bitmap.bmHeight, bitmap.bmWidthBytes / bitmap.bmWidth);
  GetBitmapBits(info.hbmColor, cursor_bitmap.byte_size(), cursor_bitmap.data());

  err_if(!DeleteObject(info.hbmColor), "failed to delete cursor information object");
  err_if(!DeleteObject(info.hbmMask), "failed to delete cursor information object");

  // get cursor position of bitmap
  auto min_x = uint32_t{};
  auto min_y = uint32_t{};
  auto max_x = uint32_t{};
  auto max_y = uint32_t{};
  auto p     = reinterpret_cast<uint8_t*>(cursor_bitmap.data());
  assert(bitmap.bmWidthBytes / bitmap.bmWidth == 4);
  for (int y = 0; y < cursor_bitmap.height; ++y)
  {
    for (int x = 0; x < cursor_bitmap.width; ++x)
    {
      auto idx = y * cursor_bitmap.row_pitch() + x * 4;
      if (p[idx] != 0 || p[idx + 1] != 0 || p[idx + 2] != 0)
      {
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x= x;
        if (y > max_y) max_y = y;
      }
    }
  }

  cursor_bitmap.pos.x = static_cast<float>(max_x - min_x) / 2 + min_x;
  cursor_bitmap.pos.y = static_cast<float>(max_y - min_y) / 2 + min_y;

  return cursor_bitmap;
}

}

////////////////////////////////////////////////////////////////////////////////
///                             Renderer
////////////////////////////////////////////////////////////////////////////////

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  auto core = Core::instance();

  // initialize resources
  core->init();
  create_pipeline_resource();

  // load gpu resources
  load_cursor_images();

  run();
}

void Renderer::destroy() noexcept
{
  Core::instance()->wait_gpu_complete();
  Core::instance()->destroy();
}

void Renderer::create_pipeline_resource() noexcept
{
  auto core = Core::instance();

  _srv_heap.init();

  // create root signature
  auto root_parameters = std::array<CD3DX12_ROOT_PARAMETER1, 3>{};
  root_parameters[0].InitAsConstants(sizeof(Constants), 0, 0, D3D12_SHADER_VISIBILITY_ALL);

  auto ranges = std::array<CD3DX12_DESCRIPTOR_RANGE1, 2>{};
  ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<uint32_t>(CursorType::Number), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
  root_parameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
  ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);
  root_parameters[2].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

  auto sampler_desc = D3D12_STATIC_SAMPLER_DESC{};
  sampler_desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.MaxLOD           = D3D12_FLOAT32_MAX;
  sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  auto signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
  signature_desc.Init_1_1(root_parameters.size(), root_parameters.data(), 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  err_if(D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  err_if(core->device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)),
          "failed to create root signature");
  
  // create shaders
  auto [vertex_shader, pixel_shader] = compile_vert_pixel("assets/shader.hlsl", "vs", "ps");

  // create pipeline state
  D3D12_INPUT_ELEMENT_DESC layout[]
  {
    { "POSITION",      0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",      0, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BUFFER_OFFSET", 0, DXGI_FORMAT_R32_UINT,     0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  auto  blend_state = D3D12_BLEND_DESC{};
  auto& rt          = blend_state.RenderTarget[0];
  rt.BlendEnable = true;
  rt.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
  rt.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
  rt.BlendOp               = D3D12_BLEND_OP_ADD;
  rt.SrcBlendAlpha         = D3D12_BLEND_ONE;
  rt.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
  rt.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
  rt.LogicOp               = D3D12_LOGIC_OP_NOOP;
  rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
  pipeline_state_desc.InputLayout           = { layout, _countof(layout) };
  pipeline_state_desc.pRootSignature        = _root_signature.Get();
  pipeline_state_desc.VS                    = CD3DX12_SHADER_BYTECODE{ vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
  pipeline_state_desc.PS                    = CD3DX12_SHADER_BYTECODE{ pixel_shader->GetBufferPointer(),  pixel_shader->GetBufferSize()  };
  pipeline_state_desc.RasterizerState       = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
  pipeline_state_desc.BlendState            = blend_state;
  pipeline_state_desc.SampleMask            = UINT_MAX;
  pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline_state_desc.NumRenderTargets      = 1;
  pipeline_state_desc.RTVFormats[0]         = DXGI_FORMAT_B8G8R8A8_UNORM;
  pipeline_state_desc.SampleDesc.Count      = 1;
  err_if(core->device()->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipeline_state)),
          "failed to create pipeline state");
}

void Renderer::load_cursor_images() noexcept
{
  auto core = Core::instance();

  // get bitmaps of all cursor types
  auto bitmaps = std::unordered_map<CursorType, Bitmap>{};
  using enum CursorType;
  bitmaps[arrow]         = load_cursor_bitmap(IDC_ARROW);
  bitmaps[up_down]       = load_cursor_bitmap(IDC_SIZENS);
  bitmaps[left_rigtht]   = load_cursor_bitmap(IDC_SIZEWE);
  bitmaps[diagonal]      = load_cursor_bitmap(IDC_SIZENESW);
  bitmaps[anti_diagonal] = load_cursor_bitmap(IDC_SIZENWSE);

  // create cursors
  for (auto& [cursor_type, bitmap] : bitmaps)
  {
    _cursors[cursor_type].image.init(bitmap.width, bitmap.height);
    _cursors[cursor_type].pos = bitmap.pos;
  }

  // create upload heap
  auto upload_heap     = ComPtr<ID3D12Resource>{};
  auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto upload_heap_des = CD3DX12_RESOURCE_DESC::Buffer(std::ranges::fold_left(_cursors | std::views::values, 0,
    [](uint32_t sum, auto const& cursor) { return sum + align(GetRequiredIntermediateSize(cursor.image.handle(), 0, 1), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT); }));
  err_if(core->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &upload_heap_des, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_heap)),
          "failed to create upload heap");
  
  // upload bitmap to cursor tesxtures
  auto offset = uint32_t{};
  for (auto const& [index, pair] : std::views::enumerate(bitmaps))
  {
    auto& [cursor_type, bitmap] = pair;
    auto& cursor_image          = _cursors[cursor_type].image;

    // upload bitmap
    auto texture_data = D3D12_SUBRESOURCE_DATA{};
    texture_data.pData      = bitmap.data();
    texture_data.RowPitch   = bitmap.width * bitmap.pixel_size;
    texture_data.SlicePitch = texture_data.RowPitch * bitmap.height;
    copy(core->cmd(), cursor_image, upload_heap.Get(), offset, texture_data);

    // convert state to pixel shader resource
    cursor_image.set_state<ImageState::pixel_shader_resource>(core->cmd());

    // move to next upload heap position
    offset += align(GetRequiredIntermediateSize(cursor_image.handle(), 0, 1), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    // create cursor texture descriptor
    cursor_image.create_descriptor(_srv_heap.pop_handle());
  }

  // create buffers
  for (auto& buf : _frame_buffers) buf.init(_srv_heap.pop_handle());

  // TODO: move to global and upload heap should be global too
  // wait gpu resources prepare complete
  core->submit(core->cmd());
  core->wait_gpu_complete();
}

void Renderer::add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept
{
  _current_frame_render_finish_procs.emplace_back([func, last_fence_value = Core::instance()->get_last_fence_value()]()
  {
    auto fence_value = Core::instance()->fence()->GetCompletedValue();
    err_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
    auto render_complete = fence_value >= last_fence_value;
    if (render_complete) func();
    return render_complete;
  });
}

void Renderer::run() noexcept
{
  // process last render finish processes
  for (auto it = _current_frame_render_finish_procs.begin(); it != _current_frame_render_finish_procs.end();)
    (*it)() ? it = _current_frame_render_finish_procs.erase(it) : ++it;

  MessageQueue::instance()->process_messages();

  update();
  render();
}

void Renderer::update() noexcept
{
  float offsets[]  = { 0, 1.f/2, 1};
  float offsets2[] = { 1, 1.f/2, 0};
  uint32_t colors[] = { 0x00ff00ff, 0x0000ffff, 0xff0000ff };
  auto i = 0;
  _shape_properties_offset = {};
  for (auto& [k, v] : _window_resources)
  {
    auto& window         = v.window;
    auto& frame_resource = v.frame_resource;
    
    frame_resource.vertices.append_range(std::vector<Vertex>
    {
      { { window.width * offsets[i],  0                           }, {}, _shape_properties_offset },
      { { window.width * offsets2[i], window.height               }, {}, _shape_properties_offset },
      { { 0,                          window.height * offsets2[i] }, {}, _shape_properties_offset },
    });
    frame_resource.indices.append_range(std::vector<uint16_t>
    {
      static_cast<uint16_t>(frame_resource.idx_beg + 0),
      static_cast<uint16_t>(frame_resource.idx_beg + 1),
      static_cast<uint16_t>(frame_resource.idx_beg + 2),
    });
    frame_resource.idx_beg += 3;

    frame_resource.shape_properties.append_range(std::vector<ShapeProperty>
    {
      { .color = colors[i] } 
    });

    _shape_properties_offset += sizeof(ShapeProperty);

    v.update();

    ++i;
  }
}

void Renderer::render() noexcept
{
  auto core = Core::instance();

  _frame_buffers[core->frame_index()].clear();
  
  for (auto& [k, wr] : _window_resources) wr.render();

  core->move_to_next_frame();
}

}}