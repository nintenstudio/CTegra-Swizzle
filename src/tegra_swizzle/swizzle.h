#pragma once

#include <tegra_swizzle/blockdepth.h>
#include <stdexcept>

// The gob address and slice size functions are ported from Ryujinx Emulator.
// https://github.com/Ryujinx/Ryujinx/blob/master/Ryujinx.Graphics.Texture/BlockLinearLayout.cs
// License MIT: https://github.com/Ryujinx/Ryujinx/blob/master/LICENSE.txt.
size_t slice_size(
    size_t block_height,
    size_t block_depth,
    size_t width_in_gobs,
    size_t height
) {
    const size_t rob_size = GOB_SIZE_IN_BYTES * block_height * block_depth * width_in_gobs;
    return div_round_up(height, block_height * GOB_HEIGHT_IN_BYTES) * rob_size;
}

size_t gob_address_z(
    size_t z,
    size_t block_height,
    size_t block_depth,
    size_t slice_size
) {
    return (z / block_depth * slice_size) + ((z & (block_depth - 1)) * GOB_SIZE_IN_BYTES * block_height);
}

size_t gob_address_y(
    size_t y,
    size_t block_height_in_bytes,
    size_t block_size_in_bytes,
    size_t image_width_in_gobs
) {
    const size_t block_y = y / block_height_in_bytes;
    const size_t block_inner_row = y % block_height_in_bytes / GOB_HEIGHT_IN_BYTES;
    return block_y * block_size_in_bytes * image_width_in_gobs + block_inner_row * GOB_SIZE_IN_BYTES;
}

// Code for offset_x and offset_y adapted from examples in the Tegra TRM page 1187.
size_t gob_address_x(size_t x, size_t block_size_in_bytes) {
    const size_t block_x = x / GOB_WIDTH_IN_BYTES;
    return block_x * block_size_in_bytes;
}

// Code taken from examples in Tegra TRM page 1188.
// Return the offset within the GOB for the byte at location (x, y).
size_t gob_offset(size_t x, size_t y) {
    // TODO: Optimize this?
    // TODO: Describe the pattern here?
    return ((x % 64) / 32) * 256 + ((y % 8) / 2) * 64 + ((x % 32) / 16) * 32 + (y % 2) * 16 + (x % 16);
}

// TODO: Investigate using macros to generate this code.
// TODO: Is it faster to use 16 byte loads for each row on incomplete GOBs?
// This may lead to better performance if the GOB is almost complete.

constexpr size_t GOB_ROW_OFFSETS[GOB_HEIGHT_IN_BYTES] = { 0, 16, 64, 80, 128, 144, 192, 208 };

void deswizzle_gob_row(unsigned char* dst, size_t dst_offset, unsigned char* src, size_t src_offset) {
    // Start with the largest offset first to reduce bounds checks.
    std::copy(src + src_offset + 288, src + src_offset + 304, dst + dst_offset + 48);
    std::copy(src + src_offset + 256, src + src_offset + 272, dst + dst_offset + 32);
    std::copy(src + src_offset + 32, src + src_offset + 48, dst + dst_offset + 16);
    std::copy(src + src_offset, src + src_offset + 16, dst + dst_offset);
}

void swizzle_gob_row(unsigned char* dst, size_t dst_offset, unsigned char* src, size_t src_offset) {
    std::copy(src + src_offset + 48, src + src_offset + 64, dst + dst_offset + 288);
    std::copy(src + src_offset + 32, src + src_offset + 48, dst + dst_offset + 256);
    std::copy(src + src_offset + 16, src + src_offset + 32, dst + dst_offset + 32);
    std::copy(src + src_offset, src + src_offset + 16, dst + dst_offset);
}

// An optimized version of the gob_offset for an entire GOB worth of bytes.
// The swizzled GOB is a contiguous region of 512 bytes.
// The deswizzled GOB is a 64x8 2D region of memory, so we need to account for the pitch.
void deswizzle_complete_gob(unsigned char* dst, unsigned char* src, size_t row_size_in_bytes) {
    // Hard code each of the GOB_HEIGHT many rows.
    // This allows the compiler to optimize the copies with SIMD instructions.
    for (size_t i = 0; i < sizeof(GOB_ROW_OFFSETS) / sizeof(GOB_ROW_OFFSETS[0]); i++) {
        deswizzle_gob_row(dst, row_size_in_bytes * i, src, GOB_ROW_OFFSETS[i]);
    }
}

// The swizzle functions are identical but with the addresses swapped.
void swizzle_complete_gob(unsigned char* dst, unsigned char* src, size_t row_size_in_bytes) {
    for (size_t i = 0; i < sizeof(GOB_ROW_OFFSETS) / sizeof(GOB_ROW_OFFSETS[0]); ++i) {
        swizzle_gob_row(dst, GOB_ROW_OFFSETS[i], src, row_size_in_bytes * i);
    }
}

/// Calculates the size in bytes for the swizzled data for the given dimensions for the block linear format.
/// The result of [swizzled_mip_size] will always be at least as large as [deswizzled_mip_size]
/// for the same surface parameters.
/// # Examples
/// Uncompressed formats like R8G8B8A8 can use the width and height in pixels.
/**
```rust
use tegra_swizzle::{BlockHeight, swizzle::swizzled_mip_size};

let width = 256;
let height = 256;
assert_eq!(262144, swizzled_mip_size(width, height, 1, BlockHeight::Sixteen, 4));
```
 */
 /// For compressed formats with multiple pixels in a block, divide the width and height by the block dimensions.
 /**
 ```rust
 # use tegra_swizzle::{BlockHeight, swizzle::swizzled_mip_size};
 // BC7 has 4x4 pixel blocks that each take up 16 bytes.
 use tegra_swizzle::div_round_up;

 let width = 256;
 let height = 256;
 assert_eq!(
     131072,
     swizzled_mip_size(div_round_up(width, 4), div_round_up(height, 4), 1, BlockHeight::Sixteen, 16)
 );
 ```
  */
size_t swizzled_mip_size(
    size_t width,
    size_t height,
    size_t depth,
    BlockHeight block_height,
    size_t bytes_per_pixel
) {
    // Assume each block is 1 GOB wide.
    const size_t _width_in_gobs = width_in_gobs(width, bytes_per_pixel);

    const size_t _height_in_blocks = height_in_blocks(height, static_cast<size_t>(block_height));
    const size_t height_in_gobs = _height_in_blocks * static_cast<size_t>(block_height);

    const size_t depth_in_gobs = round_up(depth, block_depth(depth));

    const size_t num_gobs = _width_in_gobs * height_in_gobs * depth_in_gobs;
    return num_gobs * GOB_SIZE_IN_BYTES;
}

/// Calculates the size in bytes for the deswizzled data for the given dimensions.
/// Compare with [swizzled_mip_size].
/// # Examples
/// Uncompressed formats like R8G8B8A8 can use the width and height in pixels.
/**
```rust
use tegra_swizzle::{BlockHeight, swizzle::deswizzled_mip_size};

let width = 256;
let height = 256;
assert_eq!(262144, deswizzled_mip_size(width, height, 1, 4));
```
 */
 /// For compressed formats with multiple pixels in a block, divide the width and height by the block dimensions.
 /**
 ```rust
 # use tegra_swizzle::{BlockHeight, swizzle::deswizzled_mip_size};
 // BC7 has 4x4 pixel blocks that each take up 16 bytes.
 use tegra_swizzle::div_round_up;

 let width = 256;
 let height = 256;
 assert_eq!(
     65536,
     deswizzled_mip_size(div_round_up(width, 4), div_round_up(height, 4), 1, 16)
 );
 ```
  */
size_t deswizzled_mip_size(
    size_t width,
    size_t height,
    size_t depth,
    size_t bytes_per_pixel
) {
    return width * height * depth * bytes_per_pixel;
}

template <bool DESWIZZLE>
void swizzle_deswizzle_gob(
    std::vector<unsigned char>& destination,
    std::vector<unsigned char>& source,
    size_t x0,
    size_t y0,
    size_t z0,
    size_t width,
    size_t height,
    size_t bytes_per_pixel,
    size_t gob_address
) {
    for (size_t y = 0; y < GOB_HEIGHT_IN_BYTES; ++y) {
        for (size_t x = 0; x < GOB_WIDTH_IN_BYTES; ++x) {
            if (y0 + y < height && x0 + x < width * bytes_per_pixel) {
                size_t swizzled_offset = gob_address + gob_offset(x, y);
                size_t linear_offset = (z0 * width * height * bytes_per_pixel)
                    + ((y0 + y) * width * bytes_per_pixel)
                    + x0
                    + x;

                // Swap the addresses for swizzling vs deswizzling.
                if (DESWIZZLE) {
                    destination[linear_offset] = source[swizzled_offset];
                }
                else {
                    destination[swizzled_offset] = source[linear_offset];
                }
            }
        }
    }
}

template <bool DESWIZZLE>
void swizzle_inner(
    size_t width,
    size_t height,
    size_t depth,
    std::vector<unsigned char>& source,
    std::vector<unsigned char>& destination,
    BlockHeight block_height,
    size_t block_depth,
    size_t bytes_per_pixel
) {
    const size_t _block_height = static_cast<size_t>(block_height);
    const size_t _width_in_gobs = width_in_gobs(width, bytes_per_pixel);

    const size_t _slice_size = slice_size(_block_height, block_depth, _width_in_gobs, height);

    // Blocks are always one GOB wide.
    // TODO: Citation?
    const size_t block_width = 1;
    const size_t block_size_in_bytes = GOB_SIZE_IN_BYTES * block_width * _block_height * block_depth;
    const size_t block_height_in_bytes = GOB_HEIGHT_IN_BYTES * _block_height;

    // Swizzling is defined as a mapping from byte coordinates x,y,z -> x',y',z'.
    // We step a GOB of bytes at a time to optimize the inner loop with SIMD loads/stores.
    // GOBs always use the same swizzle patterns, so we can optimize swizzling complete 64x8 GOBs.
    // The partially filled GOBs along the right and bottom edge use a slower per byte implementation.
    for (size_t z0 = 0; z0 < depth; ++z0) {
        const size_t offset_z = gob_address_z(z0, _block_height, block_depth, _slice_size);

        // Step by a GOB of bytes in y.
        for (size_t y0 = 0; y0 < height; y0 += GOB_HEIGHT_IN_BYTES) {
            const size_t offset_y = gob_address_y(
                y0,
                block_height_in_bytes,
                block_size_in_bytes,
                _width_in_gobs
            );

            // Step by a GOB of bytes in x.
            // The bytes per pixel converts pixel coordinates to byte coordinates.
            // This assumes BCN formats pass in their width and height in number of blocks rather than pixels.
            for (size_t x0 = 0; x0 < width * bytes_per_pixel; x0 += GOB_WIDTH_IN_BYTES) {
                const size_t offset_x = gob_address_x(x0, block_size_in_bytes);

                const size_t gob_address = offset_z + offset_y + offset_x;

                if (x0 + GOB_WIDTH_IN_BYTES < width * bytes_per_pixel
                    && y0 + GOB_HEIGHT_IN_BYTES < height)
                {
                    const size_t linear_offset = (z0 * width * height * bytes_per_pixel)
                        + (y0 * width * bytes_per_pixel)
                        + x0;

                    // Use optimized code to reassign bytes.
                    if (DESWIZZLE) {
                        deswizzle_complete_gob(
                            destination.data() + linear_offset,
                            source.data() + gob_address,
                            width * bytes_per_pixel
                        );
                    }
                    else {
                        swizzle_complete_gob(
                            destination.data() + gob_address,
                            source.data() + linear_offset,
                            width * bytes_per_pixel
                        );
                    }
                }
                else {

                    // There may be a row and column with partially filled GOBs.
                    // Fall back to a slow implementation that iterates over each byte.
                    swizzle_deswizzle_gob<DESWIZZLE>(
                        destination,
                        source,
                        x0,
                        y0,
                        z0,
                        width,
                        height,
                        bytes_per_pixel,
                        gob_address
                    );
                }
            }
        }
    }
}

/// Swizzles the bytes from `source` using the block linear swizzling algorithm.
///
/// Returns [SwizzleError::NotEnoughData] if `source` does not have
/// at least as many bytes as the result of [deswizzled_mip_size].
///
/// # Examples
/// Uncompressed formats like R8G8B8A8 can use the width and height in pixels.
/**
```rust
use tegra_swizzle::{BlockHeight, swizzle::deswizzled_mip_size, swizzle::swizzle_block_linear};

let width = 512;
let height = 512;
# let size = deswizzled_mip_size(width, height, 1, 4);
# let input = vec![0u8; size];
let output = swizzle_block_linear(width, height, 1, &input, BlockHeight::Sixteen, 4);
```
 */
 /// For compressed formats with multiple pixels in a block, divide the width and height by the block dimensions.
 /**
 ```rust
 # use tegra_swizzle::{BlockHeight, swizzle::deswizzled_mip_size, swizzle::swizzle_block_linear};
 // BC7 has 4x4 pixel blocks that each take up 16 bytes.
 use tegra_swizzle::div_round_up;

 let width = 512;
 let height = 512;
 # let size = deswizzled_mip_size(div_round_up(width, 4), div_round_up(height, 4), 1, 16);
 # let input = vec![0u8; size];
 let output = swizzle_block_linear(
     div_round_up(width, 4),
     div_round_up(height, 4),
     1,
     &input,
     BlockHeight::Sixteen,
     16,
 );
 ```
  */
std::vector<unsigned char> swizzle_block_linear(
    size_t width,
    size_t height,
    size_t depth,
    std::vector<unsigned char> source,
    BlockHeight block_height,
    size_t bytes_per_pixel
) {
    std::vector<unsigned char> destination(swizzled_mip_size(width, height, depth, block_height, bytes_per_pixel));
    std::fill(destination.begin(), destination.end(), (unsigned char)0);

    const size_t expected_size = deswizzled_mip_size(width, height, depth, bytes_per_pixel);
    if (source.size() < expected_size) {
        throw new std::runtime_error("Not enough data!");
    }

    // TODO: This should be a parameter since it varies by mipmap?
    const size_t _block_depth = block_depth(depth);

    swizzle_inner<false>(
        width,
        height,
        depth,
        source,
        destination,
        block_height,
        _block_depth,
        bytes_per_pixel
    );

    return destination;
}

/// Deswizzles the bytes from `source` using the block linear swizzling algorithm.
///
/// Returns [SwizzleError::NotEnoughData] if `source` does not have
/// at least as many bytes as the result of [swizzled_mip_size].
///
/// # Examples
/// Uncompressed formats like R8G8B8A8 can use the width and height in pixels.
/**
```rust
use tegra_swizzle::{BlockHeight, swizzle::swizzled_mip_size, swizzle::deswizzle_block_linear};

let width = 512;
let height = 512;
# let size = swizzled_mip_size(width, height, 1, BlockHeight::Sixteen, 4);
# let input = vec![0u8; size];
let output = deswizzle_block_linear(width, height, 1, &input, BlockHeight::Sixteen, 4);
```
 */
 /// For compressed formats with multiple pixels in a block, divide the width and height by the block dimensions.
 /**
 ```rust
 # use tegra_swizzle::{BlockHeight, swizzle::swizzled_mip_size, swizzle::deswizzle_block_linear};
 // BC7 has 4x4 pixel blocks that each take up 16 bytes.
 use tegra_swizzle::div_round_up;

 let width = 512;
 let height = 512;
 # let size = swizzled_mip_size(div_round_up(width, 4), div_round_up(height, 4), 1, BlockHeight::Sixteen, 16);
 # let input = vec![0u8; size];
 let output = deswizzle_block_linear(
     div_round_up(width, 4),
     div_round_up(height, 4),
     1,
     &input,
     BlockHeight::Sixteen,
     16,
 );
 ```
  */
std::vector<unsigned char> deswizzle_block_linear(
    size_t width,
    size_t height,
    size_t depth,
    std::vector<unsigned char> source,
    BlockHeight block_height,
    size_t bytes_per_pixel
) {
    std::vector<unsigned char> destination(deswizzled_mip_size(width, height, depth, bytes_per_pixel));
    std::fill(destination.begin(), destination.end(), (unsigned char)0);

    const size_t expected_size = swizzled_mip_size(width, height, depth, block_height, bytes_per_pixel);
    if (source.size() < expected_size) {
        throw new std::runtime_error("Not enough data!");
    }

    const size_t _block_depth = block_depth(depth);

    swizzle_inner<true>(
        width,
        height,
        depth,
        source,
        destination,
        block_height,
        _block_depth,
        bytes_per_pixel
    );

    return destination;
}