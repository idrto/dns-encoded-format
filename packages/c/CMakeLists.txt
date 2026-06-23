cmake_minimum_required(VERSION 3.16)
project(def C CXX)

add_library(def STATIC src/def.c)
target_include_directories(def PUBLIC include)

enable_testing()

add_executable(test_vectors tests/test_vectors.c)
target_link_libraries(test_vectors PRIVATE def)
add_test(NAME test_vectors COMMAND test_vectors)

add_subdirectory(../cpp cpp)
