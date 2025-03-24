cmake_minimum_required(VERSION 3.20.0)
project(GTO CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.71 COMPONENTS program_options REQUIRED)
find_package(TBB REQUIRED)
find_package(Threads REQUIRED)
find_package(benchmark REQUIRED)

add_executable(GTO main.cpp)
target_compile_options(Yolah PRIVATE -Wall -O3 -march=native -mavx2 -ffast-math -funroll-loops)

target_include_directories(Yolah PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(Yolah PRIVATE ${Boost_LIBRARIES})
target_link_libraries(Yolah PRIVATE TBB::tbb)
target_link_libraries(Yolah PRIVATE Threads::Threads)

