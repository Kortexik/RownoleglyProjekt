cmake_minimum_required(VERSION 3.16)
project(monte_carlo_openmp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Apple Clang needs the separately installed OpenMP headers and runtime.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    find_path(MC_OMP_INCLUDE omp.h HINTS
        /opt/homebrew/opt/libomp/include /usr/local/opt/libomp/include)
    find_library(MC_OMP_LIBRARY omp HINTS
        /opt/homebrew/opt/libomp/lib /usr/local/opt/libomp/lib)
    if(MC_OMP_INCLUDE AND MC_OMP_LIBRARY)
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_INCLUDE_DIR "${MC_OMP_INCLUDE}")
        set(OpenMP_CXX_LIB_NAMES omp)
        set(OpenMP_omp_LIBRARY "${MC_OMP_LIBRARY}")
    endif()
endif()
find_package(OpenMP REQUIRED COMPONENTS CXX)

add_library(mc_core src/monte_carlo.cpp)
target_compile_features(mc_core PUBLIC cxx_std_17)
target_include_directories(mc_core PUBLIC include)
target_include_directories(mc_core SYSTEM PUBLIC external/Random123/include)

add_library(mc_openmp src/omp/integrator.cpp src/omp/benchmark.cpp)
target_link_libraries(mc_openmp PUBLIC mc_core OpenMP::OpenMP_CXX)
add_library(mc_sequential src/seq/sequential.cpp)
target_link_libraries(mc_sequential PUBLIC mc_core)
foreach(target mc_core mc_openmp mc_sequential)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

add_executable(mc_omp src/omp/main.cpp src/application.cpp)
target_link_libraries(mc_omp PRIVATE mc_openmp)

add_executable(mc_seq src/seq/main.cpp src/application.cpp)
target_link_libraries(mc_seq PRIVATE mc_sequential)

add_executable(mc_experiments EXCLUDE_FROM_ALL bench/experiments.cpp)
target_link_libraries(mc_experiments PRIVATE mc_sequential mc_openmp)

include(CTest)
if(BUILD_TESTING)
    add_executable(mc_math_tests tests/test_math.cpp)
    target_link_libraries(mc_math_tests PRIVATE mc_core)
    add_test(NAME shared_mathematics COMMAND mc_math_tests)
    add_executable(mc_tests tests/test_core.cpp)
    target_link_libraries(mc_tests PRIVATE mc_openmp)
    add_test(NAME numerical_openmp COMMAND mc_tests)
    add_executable(mc_sequential_tests tests/test_sequential.cpp)
    target_link_libraries(mc_sequential_tests PRIVATE mc_sequential mc_openmp)
    add_test(NAME sequential_comparison COMMAND mc_sequential_tests)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME program_output
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_program.py"
                    "$<TARGET_FILE:mc_omp>" omp)
        add_test(NAME sequential_output
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_program.py"
                    "$<TARGET_FILE:mc_seq>" seq)
    endif()
endif()
