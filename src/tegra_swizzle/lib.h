#pragma once

#include <vector>
#include <algorithm>
#include <optional>

const size_t GOB_WIDTH_IN_BYTES = 64;
const size_t GOB_HEIGHT_IN_BYTES = 8;
const size_t GOB_SIZE_IN_BYTES = GOB_WIDTH_IN_BYTES * GOB_HEIGHT_IN_BYTES;

// Block height can only have certain values based on the Tegra TRM page 1189 table 79.

/// The height of each block in GOBs where each GOB is 8 bytes tall.
///
/// Texture file formats differ in how they encode the block height parameter.
/// Some formats may encode block height using log2, so a block height of 8 would be encoded as 3.
/// For formats that do not explicitly store block height, see [block_height_mip0].
enum class BlockHeight {
    One = 1,
    Two = 2,
    Four = 4,
    Eight = 8,
    Sixteen = 16,
    ThirtyTwo = 32,
    Invalid = 0xFFFF
};

/// Errors than can occur while swizzling or deswizzling.
enum class SwizzleError {
    NotEnoughData // C++ port: The rust implementation defines member variables for each error with details. That's a little harder to do in C++ so I'm not doing it that way.
};

constexpr BlockHeight block_height_from_value(size_t value) {
    switch (value) {
    case 1:
        return BlockHeight::One;
    case 2:
        return BlockHeight::Two;
    case 4:
        return BlockHeight::Four;
    case 8:
        return BlockHeight::Eight;
    case 16:
        return BlockHeight::Sixteen;
    case 32:
        return BlockHeight::ThirtyTwo;
    default:
        return BlockHeight::Invalid;
    }
}

/// Calculates the division of `x` by `d` but rounds up rather than truncating.
///
/// # Examples
/// Use this function when calculating dimensions for block compressed formats like BC7.
/**
```rust
# use tegra_swizzle::div_round_up;
assert_eq!(2, div_round_up(8, 4));
assert_eq!(3, div_round_up(10, 4));
```
 */
 /// Uncompressed formats are equivalent to 1x1 pixel blocks.
 /// The call to [div_round_up] can simply be ommitted in these cases.
 /**
 ```rust
 # use tegra_swizzle::div_round_up;
 let n = 10;
 assert_eq!(n, div_round_up(n, 1));
 ```
  */
constexpr size_t div_round_up(size_t x, size_t d) {
    return (x + d - 1) / d;
}

const size_t round_up(size_t x, size_t n) {
    return ((x + n - 1) / n) * n;
}

const size_t width_in_gobs(size_t width, size_t bytes_per_pixel) {
    return div_round_up(width * bytes_per_pixel, GOB_WIDTH_IN_BYTES);
}

constexpr size_t height_in_blocks(size_t height, size_t block_height) {
    // Each block is block_height many GOBs tall.
    return div_round_up(height, block_height * GOB_HEIGHT_IN_BYTES);
}

/// Just include these all here so stuff gets all linked up.
/// Not a very complicated library setup.
#include <tegra_swizzle/surface.h>