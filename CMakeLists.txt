cmake_minimum_required(VERSION 3.20)
project(NeonIcecreamBurst LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VC++ ランタイムを静的リンクし、.scr 単体で動くようにする
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(NeonIcecreamBurst WIN32
    src/main.cpp
    src/window.cpp
    src/render.cpp
    resources/app.rc
)

target_include_directories(NeonIcecreamBurst PRIVATE src resources)
target_compile_definitions(NeonIcecreamBurst PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(NeonIcecreamBurst PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8 /W4>)
endif()

# SVG を差し替えたらリソースを再コンパイルする
set_property(SOURCE resources/app.rc APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/particle.svg)

target_link_libraries(NeonIcecreamBurst PRIVATE d2d1 shlwapi)

# .scr は .exe をリネームしたもの
add_custom_command(TARGET NeonIcecreamBurst POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:NeonIcecreamBurst>
        $<TARGET_FILE_DIR:NeonIcecreamBurst>/NeonIcecreamBurst.scr
)
