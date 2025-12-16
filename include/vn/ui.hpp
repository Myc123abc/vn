#pragma once

#include <glm/glm.hpp>

#include <string_view>
#include <source_location>
#include <functional>

namespace vn { namespace ui {

////////////////////////////////////////////////////////////////////////////////
///                                Misc
////////////////////////////////////////////////////////////////////////////////

struct Color
{
  Color() = default;

  Color(uint32_t color) noexcept
  {
    r = static_cast<float>((color >> 24) & 0xFF) / 255;
    g = static_cast<float>((color >> 16) & 0xFF) / 255;
    b = static_cast<float>((color >> 8 ) & 0xFF) / 255;
    a = static_cast<float>((color      ) & 0xFF) / 255;
  }

  Color(glm::vec4 const& color) noexcept
    : r(color.r), g(color.g), b(color.b), a(color.a) {}

  operator glm::vec4() noexcept { return { r, g, b, a }; }

  float r{}, g{}, b{}, a{};
};

/**
 * get lerp color
 * @param x begin of color
 * @param y end of color
 * @param v lerp value
 * @return lerp color
 */
auto color_lerp(Color x, Color y, float v) noexcept -> glm::vec4;

/**
 * get screen size (TODO: only single monitor now)
 */
auto get_screen_size() noexcept -> glm::vec<2, uint32_t>;

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
 * @param use_title_bar
 */
void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> update_func, bool use_title_bar = true) noexcept;

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

/**
 * get content extent which expect the title bar and wireframe
 */
auto content_extent() noexcept -> std::pair<uint32_t, uint32_t>;

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
auto is_maxmized() noexcept -> bool;

/**
 * whether current window is minimize
 * @return whether minimize
 */
auto is_minimized() noexcept -> bool;

/// minimize window
void minimize_window() noexcept;

/// maximize window
void maximize_window() noexcept;

/// restore window
void restore_window() noexcept;

/**
 * set background color
 * @param color
 */
void set_background_color(Color color) noexcept;

/**
 * process a repeate event
 * @duration
 * @func func which called all the time
 * @location source location use for timer event unique distinguish
 */
void timer_repeate_event(uint32_t duration, std::function<void(float)> func, std::source_location location = std::source_location::current()) noexcept;

////////////////////////////////////////////////////////////////////////////////
///                            Shape Operator
////////////////////////////////////////////////////////////////////////////////

/**
 * set render position
 * @param x
 * @param y
 */
void set_render_pos(int x, int y) noexcept;

/// get render position
auto get_render_pos() noexcept -> glm::vec2;

/// use union operator between shapes
void begin_union() noexcept;

/**
 * end the union operator
 * @param color
 * @param thickness
 */
void end_union(Color color, float thickness = {}) noexcept;

/// use path draw between lines and beziers
void begin_path() noexcept;

/**
 * end the path draw
 * @param color
 * @param thickness
 */
void end_path(Color color = {}, float thickness = {}) noexcept;

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
void triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color color = {}, float thickness = {}) noexcept;

/**
 * draw a rectangle
 * @param left_top left upper corner
 * @param right_bottom right down corner
 * @param color
 * @param thickness
 */
void rectangle(glm::vec2 left_top, glm::vec2 right_bottom, Color color = {}, float thickness = {}) noexcept;

/**
 * draw a circle
 * @param center
 * @param radius
 * @param color
 * @param thickness
 */
void circle(glm::vec2 center, float radius, Color color = {}, float thickness = {}) noexcept;

/**
 * draw a line
 * @param p0
 * @param p1
 * @param color
 */
void line(glm::vec2 p0, glm::vec2 p1, Color color = {}) noexcept;

/**
 * draw a quadratic bezier
 * @param p0
 * @param p1
 * @param p2
 * @param color
 */
void bezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color color = 0) noexcept;

/**
 * display an image specified by position (x, y)
 * @param filename
 * @param x
 * @param y
 */
void image(std::string_view filename, int x, int y) noexcept;

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

/**
 * draw a button, can draw an icon in the center of button
 * default have a color lerp animation when cursor hover on button and leave on it
 * TODO: add bitmap draw replace draw icon by hand
 * @param x
 * @param y
 * @param width
 * @param height
 * @param button_color
 * @param button_hover_color
 * @param icon_update_func the function be called for draw icon by ui draw api
 * @param icon_width
 * @param icon_height
 * @param icon_color
 * @param icon_hover_color
 */
auto button(
  int                                     x,
  int                                     y,
  uint32_t                                width,
  uint32_t                                height,
  Color                                   button_color,
  Color                                   button_hover_color,
  std::function<void(uint32_t, uint32_t)> icon_update_func = {},
  uint32_t                                icon_width       = {},
  uint32_t                                icon_height      = {},
  Color                                   icon_color       = {},
  Color                                   icon_hover_color = {}) noexcept-> bool;

}}
