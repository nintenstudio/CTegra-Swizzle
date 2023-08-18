# CTegra-Swizzle
C++ port of https://github.com/ScanMountGoat/tegra_swizzle.

## Why does this exist?
This port exists because of a specific scenario I found myself in: a lack of Rust STD support for Nintendo Switch (I'm aware it seems silly to use this on-console but it was necessary), and no-std support being uncooperative enough that it was simpler just to port this to C++. (Plus, if I was going to have to make a modified version anyway it was better do it in C++ to simplify my build process.)
If you have need of this, feel free to use it! I probably won't be continually updating it, but I doubt that the base repository will get much in the way of updates either. It's currently in line with [6b3cf5c6fc437da68f1be68e52c9799653afabc7](https://github.com/ScanMountGoat/tegra_swizzle/commit/6b3cf5c6fc437da68f1be68e52c9799653afabc7) but is lacking extra features like testing and `nutexb_swizzle`.
