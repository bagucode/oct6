cmake_minimum_required(VERSION 3.1.1)

project(oct6)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "0")
set(VERSION_PATCH "1")
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

add_definitions(-DVERSION=${VERSION} -DDEBUG)

add_executable(${PROJECT_NAME} ../octarine.c)
