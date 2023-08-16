#pragma once

// Array alignment code ported from C# implementations of driver code by gdkchan.
// The code can be found here: https://github.com/KillzXGaming/Switch-Toolbox/pull/419#issuecomment-959980096
// This comes from the Ryujinx emulator: https://github.com/Ryujinx/Ryujinx/blob/master/LICENSE.txt.

size_t align_layer_size(
    size_t layer_size,
    size_t height,
    size_t depth,
    BlockHeight block_height_mip0,
    size_t depth_in_gobs
) {
    // Assume this is 1 based on the github comment linked above.
    // Don't support sparse textures for now.
    const int gob_blocks_in_tile_x = 1;

    size_t size = layer_size;
    size_t gob_height = static_cast<size_t>(block_height_mip0);
    size_t gob_depth = depth_in_gobs;

    if (gob_blocks_in_tile_x < 2) {
        while (height <= (gob_height / 2) * 8 && gob_height > 1) {
            gob_height /= 2;
        }

        while (depth <= (gob_depth / 2) && gob_depth > 1) {
            gob_depth /= 2;
        }

        const size_t block_of_gobs_size = gob_height * gob_depth * GOB_SIZE_IN_BYTES;
        const size_t size_in_block_of_gobs = size / block_of_gobs_size;

        if (size != size_in_block_of_gobs * block_of_gobs_size) {
            size = (size_in_block_of_gobs + 1) * block_of_gobs_size;
        }
    }
    else {
        const size_t alignment = (gob_blocks_in_tile_x * GOB_SIZE_IN_BYTES) * gob_height * gob_depth;

        size = round_up(size, alignment);
    }

    return size;
}
