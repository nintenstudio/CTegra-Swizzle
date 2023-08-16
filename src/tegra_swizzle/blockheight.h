#pragma once

// Block height code ported from C# implementations of driver code by gdkchan in Ryujinx.
// The code can be found here: https://github.com/KillzXGaming/Switch-Toolbox/pull/419#issuecomment-959980096
// License MIT: https://github.com/Ryujinx/Ryujinx/blob/master/LICENSE.txt.

/// Calculates the block height parameter to use for the first mip level if no block height is specified.
///
/// # Examples
/// Uncompressed formats like R8G8B8A8 can use the height in pixels.
/**
```rust
use tegra_swizzle::{block_height_mip0, mip_block_height};

let height = 300;
let block_height_mip0 = block_height_mip0(height);
```
 */
 /// For compressed formats with multiple pixels in a block, divide the height by the block dimensions.
 /**
 ```rust
 // BC7 has 4x4 pixel blocks that each take up 16 bytes.
 # use tegra_swizzle::{block_height_mip0, mip_block_height};
 use tegra_swizzle::{div_round_up};

 let height = 300;
 let block_height_mip0 = block_height_mip0(div_round_up(height, 4));
 ```
  */
BlockHeight block_height_mip0(size_t height) {
    const size_t height_and_half = height + (height / 2);

    if (height_and_half >= 128) {
        return BlockHeight::Sixteen;
    }
    else if (height_and_half >= 64) {
        return BlockHeight::Eight;
    }
    else if (height_and_half >= 32) {
        return BlockHeight::Four;
    }
    else if (height_and_half >= 16) {
        return BlockHeight::Two;
    }
    else {
        return BlockHeight::One;
    }
}

/// Calculates the block height parameter for the given mip level.
///
/// # Examples
/// For texture formats that don't specify the block height for the base mip level,
/// use [block_height_mip0] to calculate the initial block height.
///
/// Uncompressed formats like R8G8B8A8 can use the width and height in pixels.
/// For compressed formats with multiple pixels in a block, divide the width and height by the block dimensions.
/**
```rust
use tegra_swizzle::{block_height_mip0, div_round_up, mip_block_height};

// BC7 has 4x4 pixel blocks that each take up 16 bytes.
let height = 300;
let width = 128;
let mipmap_count = 5;

let block_height_mip0 = block_height_mip0(div_round_up(height, 4));
for mip in 0..mipmap_count {
    let mip_height = std::cmp::max(div_round_up(height >> mip, 4), 1);

    // The block height will likely change for each mip level.
    let mip_block_height = mip_block_height(mip_height, block_height_mip0);
}
```
 */
BlockHeight mip_block_height(size_t mip_height, BlockHeight block_height_mip0) {
    size_t block_height = static_cast<size_t>(block_height_mip0);
    while (mip_height <= (block_height / 2) * 8 && block_height > 1) {
        block_height /= 2;
    }

    return BlockHeight(block_height);
}