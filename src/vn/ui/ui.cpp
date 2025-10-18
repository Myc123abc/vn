#include "ui.hpp"
#include "../renderer/window_manager.hpp"
#include "ui_context.hpp"

using namespace vn::renderer;

namespace vn { namespace ui {

void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> const& update_func) noexcept
{
  UIContext::instance()->add_window(name, x, y, width, height, update_func);
}

auto window_count() noexcept -> uint32_t
{
  return WindowManager::instance()->window_count();
}

auto window_extent() noexcept -> std::pair<uint32_t, uint32_t>
{
  return { UIContext::instance()->window.width, UIContext::instance()->window.height };
}

void triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, uint32_t color) noexcept
{
  auto ctx = UIContext::instance();

  ctx->render_data.vertices.append_range(std::vector<Vertex>
  {
    { { 0,                 0                  }, {}, ctx->shape_properties_offset },
    { { ctx->window.width, 0                  }, {}, ctx->shape_properties_offset },
    { { ctx->window.width, ctx->window.height }, {}, ctx->shape_properties_offset },
    { { 0,                 ctx->window.height }, {}, ctx->shape_properties_offset },
  });
  ctx->render_data.indices.append_range(std::vector<uint16_t>
  {
    static_cast<uint16_t>(ctx->render_data.idx_beg + 0),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 1),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 2),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 0),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 2),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 3),
  });
  ctx->render_data.idx_beg += 4;

  ctx->render_data.shape_properties.emplace_back(ShapeProperty
  {
    ShapeProperty::Type::triangle,
    color,
    { p0, p1, p2 }
  });

  ctx->shape_properties_offset += ctx->render_data.shape_properties.back().byte_size();
}

}}
