# Desktop application (SDL2 + OpenGL 3.3 Core + GLAD)

add_executable(desktop_app
    src/main.cpp
)

target_include_directories(desktop_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/src/glad/include
        ${PROTO_GEN_DIR}
)

target_link_libraries(desktop_app
    PRIVATE
        engine
        SDL2::SDL2
        OpenGL::GL
        glad_gl_core_33
        ${CMAKE_DL_LIBS}
)
