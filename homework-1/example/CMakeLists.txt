cmake_minimum_required(VERSION 3.16)

project(CounterExample LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Библиотека с реализацией на C++
add_library(counter STATIC
    counter.cpp
)

target_include_directories(counter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Исполняемый файл на C
add_executable(app
    main.c
)

# Линкуем C-программу с C++ библиотекой.
# Важно: проект объявлен с LANGUAGES C CXX, поэтому CMake сам выберет корректный линкер.
target_link_libraries(app PRIVATE counter)
