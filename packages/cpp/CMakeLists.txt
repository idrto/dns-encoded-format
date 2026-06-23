cmake_minimum_required(VERSION 3.16)
project(def_cpp CXX)

add_library(def_cpp INTERFACE)
target_include_directories(def_cpp INTERFACE include)
target_link_libraries(def_cpp INTERFACE def)

enable_testing()

add_executable(test_vectors_cpp tests/test_vectors.cpp)
target_link_libraries(test_vectors_cpp PRIVATE def_cpp)
add_test(NAME test_vectors_cpp COMMAND test_vectors_cpp)
