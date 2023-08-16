#pragma once

#include <tegra_swizzle/lib.h>
#include <tegra_swizzle/swizzle.h>
#include <tegra_swizzle/blockheight.h>
#include <tegra_swizzle/arrays.h>
#include <vector>
#include <algorithm>

//! Functions for working with surfaces stored in a combined buffer for all array layers and mipmaps.
//!
//! It's common for texture surfaces to be represented
//! as a single allocated region of memory that contains all array layers and mipmaps.
//! This also applies to the swizzled surfaces used for textures on the Tegra X1.
//!
//! Use [deswizzle_surface] for reading swizzled surfaces into a single deswizzled `Vec<u8>`.
//! This output can be used as is for creating DDS files.
//! Modern graphics APIs like Vulkan also support this dense layout for initializing all
//! array layers and mipmaps for a texture in a single API call.
//!
//! Use [swizzle_surface] for writing a swizzled surface from a combined buffer like the result of [deswizzle_surface] or a DDS file.
//! The result of [swizzle_surface] is the layout expected for many texture file formats for console games targeting the Tegra X1.
//!
//! # Examples
//! Array layers and mipmaps are ordered by layer and then mipmap.
//! A surface with `L` layers and `M` mipmaps would have the following layout.
/*!
```no_compile
Layer 0 Mip 0
Layer 0 Mip 1
...
Layer 0 Mip M-1
Layer 1 Mip 0
Layer 1 Mip 1
...
Layer L-1 Mip M-1
```
*/
//! The convention is for the non swizzled layout to be tightly packed.
//! Swizzled surfaces add additional padding and alignment between layers and mipmaps.

/// The dimensions of a compressed block. Compressed block sizes are usually 4x4 pixels.
struct BlockDim {
    size_t width;
    size_t height;
    size_t depth;
};

/// A 1x1x1 block for formats that do not use block compression like R8G8B8A8.
constexpr BlockDim block_dim_uncompressed() {
    BlockDim blockDim;
    blockDim.width = 1;
    blockDim.height = 1;
    blockDim.depth = 1;
    return blockDim;
}

/// A 4x4x1 compressed block. This includes any of the BCN formats like BC1, BC3, or BC7.
/// This also includes DXT1, DXT3, and DXT5.
constexpr BlockDim block_dim_4x4() {
    BlockDim blockDim;
    blockDim.width = 4;
    blockDim.height = 4;
    blockDim.depth = 1;
    return blockDim;
}

// TODO: Add examples.
/// Calculates the size in bytes for the swizzled data for the given surface.
/// Compare with [deswizzled_surface_size].
///
/// Dimensions should be in pixels.
///
/// Set `block_height_mip0` to [None] to infer the block height from the specified dimensions.
size_t swizzled_surface_size(
    size_t width,
    size_t height,
    size_t depth,
    BlockDim block_dim, // TODO: Use None to indicate uncompressed?
    std::optional<BlockHeight> _block_height_mip0, // TODO: Make this optional in other functions as well?
    size_t bytes_per_pixel,
    size_t mipmap_count,
    size_t layer_count
) {
    size_t block_width = block_dim.width;
    size_t block_height = block_dim.height;
    size_t block_depth = block_dim.depth;

    // The block height can be inferred if not specified.
    // TODO: Enforce a block height of 1 for depth textures elsewhere?
    BlockHeight __block_height_mip0 = (depth == 1) ? _block_height_mip0.value_or(block_height_mip0(div_round_up(height, block_height))) : BlockHeight::One;

    size_t mip_size = 0;
    for (size_t mip = 0; mip < mipmap_count; ++mip) {
        size_t mip_width = std::max(div_round_up(width >> mip, block_width), (size_t)1);
        size_t mip_height = std::max(div_round_up(height >> mip, block_height), (size_t)1);
        size_t mip_depth = std::max(div_round_up(depth >> mip, block_depth), (size_t)1);
        BlockHeight _mip_block_height = mip_block_height(mip_height, __block_height_mip0);

        mip_size += swizzled_mip_size(
            mip_width,
            mip_height,
            mip_depth,
            _mip_block_height,
            bytes_per_pixel
        );
    }

    if (layer_count > 1) {
        // We only need alignment between layers.
        size_t layer_size = align_layer_size(mip_size, height, depth, __block_height_mip0, 1);
        return layer_size * layer_count;
    }
    else {
        return mip_size;
    }
}

// TODO: Add examples.
/// Calculates the size in bytes for the deswizzled data for the given surface.
/// Compare with [swizzled_surface_size].
///
/// Dimensions should be in pixels.
size_t deswizzled_surface_size(
    size_t width,
    size_t height,
    size_t depth,
    BlockDim block_dim,
    size_t bytes_per_pixel,
    size_t mipmap_count,
    size_t layer_count
) {
    // TODO: Avoid duplicating this code.
    const size_t block_width = block_dim.width;
    const size_t block_height = block_dim.height;
    const size_t block_depth = block_dim.depth;

    size_t layer_size = 0;
    for (size_t mip = 0; mip < mipmap_count; mip++) {
        size_t mip_width = std::max(div_round_up(width >> mip, block_width), (size_t)1);
        size_t mip_height = std::max(div_round_up(height >> mip, block_height), (size_t)1);
        size_t mip_depth = std::max(div_round_up(depth >> mip, block_depth), (size_t)1);
        layer_size += deswizzled_mip_size(mip_width, mip_height, mip_depth, bytes_per_pixel);
    }

    return layer_size * layer_count;
}

template <bool DESWIZZLE>
std::vector<unsigned char> surface_destination(
    size_t width,
    size_t height,
    size_t depth,
    BlockDim block_dim,
    std::optional<BlockHeight> block_height_mip0,
    size_t bytes_per_pixel,
    size_t mipmap_count,
    size_t layer_count,
    std::vector<unsigned char>& source
) {
    size_t swizzled_size = swizzled_surface_size(
        width,
        height,
        depth,
        block_dim,
        block_height_mip0,
        bytes_per_pixel,
        mipmap_count,
        layer_count
    );
    size_t deswizzled_size = deswizzled_surface_size(
        width,
        height,
        depth,
        block_dim,
        bytes_per_pixel,
        mipmap_count,
        layer_count
    );

    size_t surface_size = DESWIZZLE ? deswizzled_size : swizzled_size;
    size_t expected_size = DESWIZZLE ? swizzled_size : deswizzled_size;

    // Validate the source length before attempting to allocate.
    // This reduces potential out of memory panics.
    if (source.size() < expected_size) {
        throw std::runtime_error("Not enough data!");
    }

    // Assume the calculated size is accurate, so don't reallocate later.
    std::vector<unsigned char> result;
    result.resize(surface_size, (unsigned char)0);
    return result;
}

template <bool DESWIZZLE>
void swizzle_mipmap(
    size_t width,
    size_t height,
    size_t depth,
    BlockHeight block_height,
    size_t block_depth,
    size_t bytes_per_pixel,
    const std::vector<unsigned char>& source,
    size_t& src_offset,
    std::vector<unsigned char>& dst,
    size_t& dst_offset
) {
    size_t swizzled_size = swizzled_mip_size(width, height, depth, block_height, bytes_per_pixel);
    size_t deswizzled_size = deswizzled_mip_size(width, height, depth, bytes_per_pixel);

    // Make sure the source has enough space.
    if (DESWIZZLE && source.size() < src_offset + swizzled_size) {
        throw std::runtime_error("Not enough data!");
    }

    if (!DESWIZZLE && source.size() < src_offset + deswizzled_size) {
        throw std::runtime_error("Not enough data!");
    }

    std::vector part_of_source(source.begin() + src_offset, source.end());
    std::vector part_of_dst(dst.begin() + dst_offset, dst.end());

    // Swizzle the data and move to the next section.
    swizzle_inner<DESWIZZLE>(
        width,
        height,
        depth,
        part_of_source,
        part_of_dst,
        block_height,
        block_depth,
        bytes_per_pixel
    );

    // TODO: Find a faster C++ implementation than copying
    for (size_t i = 0; i < part_of_dst.size(); i++)
        dst[i + dst_offset] = part_of_dst[i];

    if (DESWIZZLE) {
        src_offset += swizzled_size;
        dst_offset += deswizzled_size;
    }
    else {
        src_offset += deswizzled_size;
        dst_offset += swizzled_size;
    }
}

template <bool DESWIZZLE>
void swizzle_surface_inner(
    size_t width,
    size_t height,
    size_t depth,
    std::vector<unsigned char>& source,
    std::vector<unsigned char>& result,
    BlockDim block_dim,
    std::optional<BlockHeight> _block_height_mip0, // TODO: Make this optional in other functions as well?
    size_t bytes_per_pixel,
    size_t mipmap_count,
    size_t layer_count
) {
    size_t block_width = block_dim.width;
    size_t block_height = block_dim.height;
    size_t _block_depth = block_dim.depth;

    // The block height can be inferred if not specified.
    // TODO: Enforce a block height of 1 for depth textures elsewhere?
    BlockHeight __block_height_mip0 = (depth == 1) ? _block_height_mip0.value_or(block_height_mip0(div_round_up(height, block_height))) : BlockHeight::One;

    // TODO: Don't assume block_depth is 1?
    const size_t block_depth_mip0 = block_depth(depth);

    size_t src_offset = 0;
    size_t dst_offset = 0;
    for (size_t i = 0; i < layer_count; ++i) {
        for (size_t mip = 0; mip < mipmap_count; ++mip) {
            size_t mip_width = std::max(div_round_up(width >> mip, block_width), (size_t)1);
            size_t mip_height = std::max(div_round_up(height >> mip, block_height), (size_t)1);
            size_t mip_depth = std::max(div_round_up(depth >> mip, _block_depth), (size_t)1);

            BlockHeight _mip_block_height = mip_block_height(mip_height, __block_height_mip0);
            size_t _mip_block_depth = mip_block_depth(mip_depth, block_depth_mip0);

            swizzle_mipmap<DESWIZZLE>(
                mip_width,
                mip_height,
                mip_depth,
                _mip_block_height,
                _mip_block_depth,
                bytes_per_pixel,
                source,
                src_offset,
                result,
                dst_offset
            );
        }

        // Align offsets between array layers.
        if (layer_count > 1) {
            if (DESWIZZLE) {
                src_offset = align_layer_size(src_offset, height, depth, __block_height_mip0, 1);
            }
            else {
                dst_offset = align_layer_size(dst_offset, height, depth, __block_height_mip0, 1);
            }
        }
    }
}

/// Swizzles all the array layers and mipmaps in `source` using the block linear algorithm
/// to a combined vector with appropriate mipmap and layer alignment.
///
/// Returns [SwizzleError::NotEnoughData] if `source` does not have
/// at least as many bytes as the result of [deswizzled_surface_size].
///
/// Set `block_height_mip0` to [None] to infer the block height from the specified dimensions.
std::vector<unsigned char> swizzle_surface(
    size_t width,
    size_t height,
    size_t depth,
    std::vector<unsigned char>& source,
    BlockDim block_dim, // TODO: Use None to indicate uncompressed?
    std::optional<BlockHeight> block_height_mip0, // TODO: Make this optional in other functions as well?
    size_t bytes_per_pixel,
    size_t mipmap_count,
    size_t layer_count
) {
    std::vector<unsigned char> result = surface_destination<false>(
        width,
        height,
        depth,
        block_dim,
        block_height_mip0,
        bytes_per_pixel,
        mipmap_count,
        layer_count,
        source
    );

    swizzle_surface_inner<false>(
        width,
        height,
        depth,
        source,
        result,
        block_dim,
        block_height_mip0,
        bytes_per_pixel,
        mipmap_count,
        layer_count
    );

    return result;
}

// TODO: Find a way to simplify the parameters.
/// Deswizzles all the array layers and mipmaps in `source` using the block linear algorithm
/// to a new vector without any padding between layers or mipmaps.
///
/// Returns [SwizzleError::NotEnoughData] if `source` does not have
/// at least as many bytes as the result of [swizzled_surface_size].
///
/// Set `block_height_mip0` to [None] to infer the block height from the specified dimensions.
std::vector<unsigned char> deswizzle_surface(
    size_t width,
    size_t height,
    size_t depth,
    std::vector<unsigned char>& source,
    BlockDim block_dim,
    std::optional<BlockHeight> block_height_mip0, // TODO: Make this optional in other functions as well?
    size_t bytes_per_pixel,
    size_t mipmap_count,
    size_t layer_count
) {
    std::vector<unsigned char> result = surface_destination<true>(
        width,
        height,
        depth,
        block_dim,
        block_height_mip0,
        bytes_per_pixel,
        mipmap_count,
        layer_count,
        source
    );

    swizzle_surface_inner<true>(
        width,
        height,
        depth,
        source,
        result,
        block_dim,
        block_height_mip0,
        bytes_per_pixel,
        mipmap_count,
        layer_count
    );

    return result;
}