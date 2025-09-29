cmake_minimum_required(VERSION 3.20.0)
project(GTO CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.71 COMPONENTS program_options REQUIRED)
find_package(TBB REQUIRED)
find_package(Threads REQUIRED)
find_package(benchmark REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(GTO main.cpp)
target_compile_options(GTO PRIVATE -Wall -march=native -mavx2 -ffast-math -funroll-loops)

target_include_directories(GTO PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(GTO PRIVATE ${Boost_LIBRARIES})
target_link_libraries(GTO PRIVATE TBB::tbb)
target_link_libraries(GTO PRIVATE Threads::Threads)
target_link_libraries(GTO PRIVATE OpenMP::OpenMP_CXX)
