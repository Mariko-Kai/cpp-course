cmake_minimum_required(VERSION 3.10)
project(TaskScheduler C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Создаем статическую библиотеку из C++ кода
add_library(task_scheduler_lib STATIC task_scheduler.cpp)

# Создаем исполняемый файл тестов на C
add_executable(test_scheduler test.c)

# Линкуем библиотеку с тестами
target_link_libraries(test_scheduler PRIVATE task_scheduler_lib)
