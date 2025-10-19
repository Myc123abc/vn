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
 * draw a triangle (clockwise)
 * @param p1
 * @param p2
 * @param p3
 * @param color
 * @param thickness
 */
void triangle(glm::vec2 const& p0, glm::vec2 const& p1, glm::vec2 const& p2, uint32_t color, float thickness = {}) noexcept;

/**
 * draw a rectangle
 * @param left_top left upper corner
 * @param right_bottom right down corner
 * @param color
 * @param thickness
 */
void rectangle(glm::vec2 const& left_top, glm::vec2 const& right_bottom, uint32_t color, uint32_t thickness = {}) noexcept;

}}
