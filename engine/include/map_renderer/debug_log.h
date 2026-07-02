#pragma once

#ifdef MAP_RENDERER_DEBUG
    #include <cstdio>
    #define DEBUG_LOG(fmt, ...) \
        std::fprintf(stderr, "[map_renderer] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) ((void)0)
#endif
