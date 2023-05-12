add_library(glad STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gl.c
)
target_compile_options(glad PRIVATE -fPIC)
target_include_directories(glad PUBLIC include)

