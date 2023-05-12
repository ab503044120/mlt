cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)
add_executable(test1 test1.cpp)
target_link_libraries(test1 PUBLIC mlt++ pthread)

add_executable(test2 test2.cpp)
target_link_libraries(test2 PUBLIC mlt++ pthread)

add_executable(test3 test3.cpp)
target_link_libraries(test3 PUBLIC mlt++ pthread)

add_executable(test4 test4.cpp)
target_link_libraries(test4 PUBLIC mlt++ pthread)

add_executable(glxgears glxgears.cpp)
target_link_libraries(glxgears PUBLIC -lGL -lGLEW -lGLU -lGL -lm -lX11 -lXext)

add_executable(test_movit test_movit.cpp)
target_link_libraries(test_movit PUBLIC Threads::Threads OpenGL::GL PkgConfig::movit PkgConfig::sdl2)

