#pragma once

constexpr size_t block_depth(size_t depth) {
    // TODO: Should this be an enum similar to BlockHeight?
    // This would only matter if it was part of the public API.
    const size_t depth_and_half = depth + (depth / 2);
    if (depth_and_half >= 16) {
        return 16;
    }
    else if (depth_and_half >= 8) {
        return 8;
    }
    else if (depth_and_half >= 4) {
        return 4;
    }
    else if (depth_and_half >= 2) {
        return 2;
    }
    else {
        return 1;
    }
}

size_t mip_block_depth(size_t mip_depth, size_t gob_depth) {
    while (mip_depth <= gob_depth / 2 && gob_depth > 1) {
        gob_depth /= 2;
    }

    return gob_depth;
}