# Desktop application (SDL2 + OpenGL 3.3)

add_executable(desktop_app
    src/main.cpp
)

target_link_libraries(desktop_app
    PRIVATE
        engine
        SDL2::SDL2
        OpenGL::GL
        glad_gl_core_33
)
