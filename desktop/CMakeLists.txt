find_package(FLTK REQUIRED)

add_executable(mapview
    src/main.cpp
    src/MapWidget.cpp
)

target_include_directories(mapview
    PRIVATE
        ${FLTK_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mapview
    PRIVATE
        maprender
        ${FLTK_LIBRARIES}
)
