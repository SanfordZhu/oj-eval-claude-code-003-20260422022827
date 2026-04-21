cmake_minimum_required(VERSION 3.10)
project(icpc_management_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags for optimization and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wpedantic -Werror")

# Create executable
add_executable(code main.cpp)

# Set output name to 'code' as required
set_target_properties(code PROPERTIES OUTPUT_NAME "code")