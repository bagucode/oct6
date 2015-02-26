cmake_minimum_required(VERSION 3.1.1)

project(oct6)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "0")
set(VERSION_PATCH "1")
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SRC_DIR ../src/)

set(LLVM_DIR "Please set LLVM directory" CACHE PATH "LLVM build or install directory")

file(GLOB SOURCES "${SRC_DIR}*.h" "${SRC_DIR}*.c" "${SRC_DIR}*.cpp")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})

add_definitions(-DVERSION=${VERSION} ${LLVM_DEFINITIONS})

add_executable(${PROJECT_NAME} ${SOURCES})

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

