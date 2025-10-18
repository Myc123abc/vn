#pragma once

#include <string_view>

namespace vn { namespace ui {

/**
 * create a window
 * @param name cannot be duplicated
 * @x position x of window
 * @y position y of window
 * @width width of window
 * @height height of window
 */
void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

/**
 * get count of windows
 * @return count of windows
 */
auto window_count() noexcept -> uint32_t;

}}
