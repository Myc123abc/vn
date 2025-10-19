#pragma once

#include <glm/glm.hpp>

#include <string_view>
#include <functional>

namespace vn { namespace ui {

/**
 * create a window
 * @param name cannot be duplicated
 * @x position x of window
 * @y position y of window
 * @width width of window
 * @height height of window
 * @update_func update function for rendering
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

/**
 * draw a triangle
 * @param p1 positon 0 of triangle
 * @param p2 positon 1 of triangle
 * @param p3 positon 2 of triangle
 * @param color color of triangle
 * @param thickness thickness of triangle
 */
void triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, uint32_t color, float thickness = {}) noexcept;

}}
