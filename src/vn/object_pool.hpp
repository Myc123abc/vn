#pragma once

#include "error_handling.hpp"

#include <array>
#include <vector>
#include <memory>
#include <assert.h>
#include <algorithm>

namespace vn {

template <typename T, uint32_t BlockCapacity>
requires (BlockCapacity > 0)                                     &&
         (BlockCapacity <= std::numeric_limits<uint16_t>::max()) &&
         std::is_nothrow_constructible_v<T>                      &&
         std::is_nothrow_destructible_v<T>
class ObjectPool
{
public:
  class [[nodiscard]] Handle
  {
    friend class ObjectPool;
  public:
    constexpr Handle() noexcept = default;
  private:
    constexpr Handle(uint16_t block_idx, uint16_t slot_idx, uint32_t generation) noexcept
      : _block_idx(block_idx), _slot_idx(slot_idx), _generation(generation) {}

  public:
    constexpr auto valid() const noexcept { return _generation != 0; }

  private:
    uint16_t _block_idx{};
    uint16_t _slot_idx{};
    uint32_t _generation{};
  };

  ObjectPool() noexcept
  {
    _blocks.emplace_back(std::make_unique<Block>());
  }

  ~ObjectPool() noexcept
  {
    err_if(std::ranges::any_of(_blocks, [](auto const& block)
    {
      return std::ranges::any_of(*block, [](auto const& slot) { return slot.alive; });
    }), "[ObjectPool] Failed to destruct ObjectPool. Still have objects are undestroied");
  }

  ObjectPool(ObjectPool const&)            = delete;
  ObjectPool(ObjectPool&&)                 = delete;
  ObjectPool& operator=(ObjectPool const&) = delete;
  ObjectPool& operator=(ObjectPool&&)      = delete;

  [[nodiscard]]
  auto create() noexcept
  {
    // create new one if free list is empty
    if (_free_list.empty())
    {
      auto slot = get_slot(_block_idx, _slot_idx);
      assert(!slot->alive && !slot->generation);
      new (&slot->obj) T{};
      slot->alive      = true;
      slot->generation = 1;
      auto handle = Handle{ _block_idx, _slot_idx, slot->generation };
      if (++_slot_idx == BlockCapacity)
      {
        _slot_idx = 0;
        _blocks.emplace_back(std::make_unique<Block>());
        err_if(_block_idx + 1 == std::numeric_limits<uint16_t>::max(),
          "[ObjectPool] Failed to allocate new block, exceed the max block capacity");
        ++_block_idx;
      }
      return handle;
    }

    // use free list
    auto& free_slot = _free_list.back(); 
    _free_list.pop_back();
    auto slot = get_slot(free_slot.block_idx, free_slot.slot_idx);
    assert(!slot->alive && slot->generation);
    new (&slot->obj) T{};
    slot->alive = true;
    return Handle{ free_slot.block_idx, free_slot.slot_idx, slot->generation };
  }

  [[nodiscard]]
  auto get(Handle handle) noexcept -> T*
  {
    auto slot = get_slot(handle._block_idx, handle._slot_idx);
    assert(handle.valid() && slot->alive && slot->generation == handle._generation);
    return slot->get();
  }

  [[nodiscard]]
  auto get(Handle handle) const noexcept -> T const*
  {
    auto slot = get_slot(handle._block_idx, handle._slot_idx);
    assert(handle.valid() && slot->alive && slot->generation == handle._generation);
    return slot->get();
  }

  void destroy(Handle& handle) noexcept
  {
    auto slot = get_slot(handle._block_idx, handle._slot_idx);
    assert(handle.valid() && slot->alive && slot->generation == handle._generation);
    slot->get()->~T();
    slot->alive = false;
    ++slot->generation;
    err_if(slot->generation == std::numeric_limits<decltype(slot->generation)>::max(),
      "[ObjectPool] Failed to destroy object, exceed the max slot generation");
    _free_list.emplace_back(handle._block_idx, handle._slot_idx);
    handle = {};
  }

private:
  auto get_slot(uint16_t block_idx, uint16_t slot_idx) const noexcept
  {
    assert(block_idx < _blocks.size() && slot_idx < BlockCapacity);
    return &(*_blocks[block_idx])[slot_idx];
  }

private:
  struct Slot
  {
    alignas(T) std::byte obj[sizeof(T)];
    uint32_t             generation{};
    bool                 alive{};

    auto get() noexcept -> T*
    {
      return std::launder(reinterpret_cast<T*>(obj));
    }

    auto get() const noexcept -> T const*
    {
      return std::launder(reinterpret_cast<T const*>(obj));
    }
  };

  using Block = std::array<Slot, BlockCapacity>;

  struct FreeSlot
  {
    uint16_t block_idx{};
    uint16_t slot_idx{};
  };

  std::vector<std::unique_ptr<Block>> _blocks;
  std::vector<FreeSlot>               _free_list;
  uint16_t                            _block_idx{};
  uint16_t                            _slot_idx{};
};

}
