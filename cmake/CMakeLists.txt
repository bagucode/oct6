cmake_minimum_required(VERSION 3.1.1)

project(oct6)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "0")
set(VERSION_PATCH "1")
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(LLVM_DIR SET_LLVM_DIR_HERE)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})

add_definitions(-DVERSION=${VERSION} -DDEBUG ${LLVM_DEFINITIONS})

add_executable(${PROJECT_NAME} ../src/common.h ../src/Object.h ../src/octarine.c)

cmake_minimum_required(VERSION 2.8.8)
project(SimpleProject)


# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

# include_directories(${LLVM_INCLUDE_DIRS})
# add_definitions(${LLVM_DEFINITIONS})

# Now build our tools
# add_executable(simple-tool tool.cpp)

# Find the libraries that correspond to the LLVM components
# that we wish to use
# llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
# target_link_libraries(simple-tool ${llvm_libs})