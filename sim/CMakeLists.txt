cmake_minimum_required(VERSION 3.16)
project(jc_sim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LVGL ${ROOT}/.pio/libdeps/jc4827w543/lvgl)
set(AJSON ${ROOT}/.pio/libdeps/jc4827w543/ArduinoJson/src)

file(GLOB_RECURSE LVGL_SRC ${LVGL}/src/*.c)

add_executable(sim
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_sim_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/link_stub.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/serial_tcp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/settings_store_stub.cpp
    ${ROOT}/src/ui_home.cpp
    ${ROOT}/src/ui_now_playing.cpp
    ${ROOT}/src/link.cpp
    ${ROOT}/src/i18n.cpp
    ${ROOT}/src/opensans_16.c
    ${ROOT}/src/icons.c
    ${ROOT}/src/ui_queue.cpp
    ${ROOT}/src/ui_settings.cpp
    ${ROOT}/src/ui_playlist.cpp
    ${LVGL_SRC}
)

target_compile_definitions(sim PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_include_directories(sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ROOT}/include
    ${ROOT}/src
    ${LVGL}
    ${AJSON}
)

find_package(SDL2 REQUIRED)
target_link_libraries(sim PRIVATE SDL2::SDL2main SDL2::SDL2 ws2_32)
