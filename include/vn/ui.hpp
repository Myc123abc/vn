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
void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> const& update_func) noexcept;

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

////////////////////////////////////////////////////////////////////////////////
///                            Shape Operator
////////////////////////////////////////////////////////////////////////////////

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
void triangle(glm::vec2 const& p0, glm::vec2 const& p1, glm::vec2 const& p2, uint32_t color = {}, float thickness = {}) noexcept;

/**
 * draw a rectangle
 * @param left_top left upper corner
 * @param right_bottom right down corner
 * @param color
 * @param thickness
 */
void rectangle(glm::vec2 const& left_top, glm::vec2 const& right_bottom, uint32_t color = {}, float thickness = {}) noexcept;

/**
 * draw a circle
 * @param center
 * @param radius
 * @param color
 * @param thickness
 */
void circle(glm::vec2 const& center, float radius, uint32_t color = {}, float thickness = {}) noexcept;

/**
 * draw a line
 * @param p0
 * @param p1
 * @param color
 */
void line(glm::vec2 const& p0, glm::vec2 const& p1, uint32_t color = {}) noexcept;

/**
 * draw a quadratic bezier
 * @param p0
 * @param p1
 * @param p2
 * @param color
 */
void bezier(glm::vec2 const& p0, glm::vec2 const& p1, glm::vec2 const& p2, uint32_t color = 0) noexcept;

////////////////////////////////////////////////////////////////////////////////
///                              UI Widget
////////////////////////////////////////////////////////////////////////////////

auto is_hover_on(glm::vec2 const& left_top, glm::vec2 const& right_bottom) noexcept -> bool;

}}
