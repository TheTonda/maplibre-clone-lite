#pragma once

#include "debug_log.h"

#ifdef MAP_RENDERER_DEBUG
    // GL_CHECK takes a const GLFunctions& so it can be used in any scope,
    // not just Renderer methods. Uses standard C++ types (no GL headers
    // needed) — glGetError returns uint32_t, 0 means no error.
    #define GL_CHECK(gl) \
        do { \
            uint32_t err = (gl).glGetError(); \
            if (err != 0) { \
                DEBUG_LOG("GL error 0x%x at %s:%d", err, __FILE__, __LINE__); \
            } \
        } while (0)
#else
    #define GL_CHECK(gl) ((void)0)
#endif
