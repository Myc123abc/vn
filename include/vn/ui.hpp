#pragma once

#include <glm/glm.hpp>

#include <string_view>
#include <functional>

namespace vn { namespace ui {

////////////////////////////////////////////////////////////////////////////////
///                                Window
////////////////////////////////////////////////////////////////////////////////

/**
 * create a window
 * @param name cannot be duplicated
 * @param x position x of window
 * @param y position y of window
 * @param width width of window
 * @param height height of window
 * @param update_func update function for rendering
 */
void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> update_func) noexcept;

/**
 * get count of windows
 * @return count of windows
 */
auto window_count() noexcept -> uint32_t;

/**
 * get window extent in current update function of window
 * @return extent of window
 */
auto window_extent() noexcept -> std::pair<uint32_t, uint32_t>;

/// close current window
void close_window() noexcept;

/**
 * set cursor move invalid area
 * every rendering will be reset
 * @param x
 * @param y
 * @param width
 * @param height
 */
void add_move_invalid_area(glm::vec2 left_top, glm::vec2 right_bottom) noexcept;

/**
 * whether current window is active
 * @return whether active
 */
auto is_active() noexcept -> bool;

/**
 * whether current window is moving
 * @return whether moving
 */
auto is_moving() noexcept -> bool;

/**
 * whether current window is resizing
 * @return whether resizing
 */
auto is_resizing() noexcept -> bool;

/**
 * whether current window is maximize
 * @return whether maxmize
 */
auto is_maxmize() noexcept -> bool;

/**
 * whether current window is minimize
 * @return whether minimize
 */
auto is_minimize() noexcept -> bool;

/// minimize window
void minimize_window() noexcept;

/// maximize window
void maximize_window() noexcept;

/// restore window
void restore_window() noexcept;

////////////////////////////////////////////////////////////////////////////////
///                            Shape Operator
////////////////////////////////////////////////////////////////////////////////

/**
 * set render position. TODO: default position relevant with window style (title bar | wireframe | maximize)
 * @param x
 * @param y
 */
void set_render_pos(int x, int y) noexcept;

/// get render position
auto get_render_pos() noexcept -> glm::vec2;

/// temporary set render position macro
/// example:
///   Tmp_Render_Pos(0, 0)
///   {
///     // draw functions
///   }
#define Tmp_Render_Pos(__x, __y) \
  for (auto __call_once = true; __call_once;) \
    for (auto __old_render_pos = get_render_pos(); __call_once; set_render_pos(__old_render_pos.x, __old_render_pos.y)) \
      for (set_render_pos(__x, __y); __call_once; __call_once = false)

// FIXME: use in internal
/**
 * enable temporary color, use for global, independent windows
 * @param color
 */
void enable_tmp_color(uint32_t color) noexcept;

// FIXME: use in internal
/// disable temporary color
void disable_tmp_color() noexcept;

// FIXME: use in internal
/**
 * lerp color from color_beg to color_end for last shape
 * @param color_beg
 * @param color_end
 * @param value lerp value, from 0.0 ~ 1.0
 */
void lerp_color(uint32_t color_beg, uint32_t color_end, float value) noexcept;

/// use union operator between shapes
void begin_union() noexcept;

/**
 * end the union operator
 * @param color
 * @param thickness
 */
void end_union(uint32_t color, float thickness = {}) noexcept;

/// use path draw between lines and beziers
void begin_path() noexcept;

/**
 * end the path draw
 * @param color
 * @param thickness
 */
void end_path(uint32_t color = {}, float thickness = {}) noexcept;

/**
 * discard the pixel of specific rectangle for last draw shape
 * @param left_top
 * @param right_bottom
 */
void discard_rectangle(glm::vec2 left_top, glm::vec2 right_bottom) noexcept;

////////////////////////////////////////////////////////////////////////////////
///                            Basic Shape
////////////////////////////////////////////////////////////////////////////////

/**
 * draw a triangle (clockwise)
 * @param p1
 * @param p2
 * @param p3
 * @param color
 * @param thickness
 */
void triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, uint32_t color = {}, float thickness = {}) noexcept;

/**
 * draw a rectangle
 * @param left_top left upper corner
 * @param right_bottom right down corner
 * @param color
 * @param thickness
 */
void rectangle(glm::vec2 left_top, glm::vec2 right_bottom, uint32_t color = {}, float thickness = {}) noexcept;

/**
 * draw a circle
 * @param center
 * @param radius
 * @param color
 * @param thickness
 */
void circle(glm::vec2 center, float radius, uint32_t color = {}, float thickness = {}) noexcept;

/**
 * draw a line
 * @param p0
 * @param p1
 * @param color
 */
void line(glm::vec2 p0, glm::vec2 p1, uint32_t color = {}) noexcept;

/**
 * draw a quadratic bezier
 * @param p0
 * @param p1
 * @param p2
 * @param color
 */
void bezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, uint32_t color = 0) noexcept;

////////////////////////////////////////////////////////////////////////////////
///                              UI Widget
////////////////////////////////////////////////////////////////////////////////

/**
 * whether cursor hover on specific region
 * @param left_top
 * @param right_bottom
 */
auto is_hover_on(glm::vec2 left_top, glm::vec2 right_bottom) noexcept -> bool;

/**
 * whether cursor click on specific region
 * @param left_top
 * @param right_bottom
 */
auto is_click_on(glm::vec2 left_top, glm::vec2 right_bottom) noexcept -> bool;

auto button(
  uint32_t                                x,
  uint32_t                                y,
  uint32_t                                width,
  uint32_t                                height,
  uint32_t                                button_color,
  uint32_t                                button_hover_color,
  std::function<void(uint32_t, uint32_t)> icon_update_func = {},
  uint32_t                                icon_width       = {},
  uint32_t                                icon_height      = {},
  uint32_t                                icon_color       = {},
  uint32_t                                icon_hover_color = {}) noexcept-> bool;

}}
