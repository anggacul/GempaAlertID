cmake_minimum_required(VERSION 3.10)
project(PickSeisAI C)

set(CMAKE_C_STANDARD 11)

# OpenMP
find_package(OpenMP REQUIRED)

include_directories(include)

file(GLOB CORE_SRC src/*.c)
add_executable(pickseisai pickseisai.c ${CORE_SRC})

target_link_libraries(pickseisai PRIVATE OpenMP::OpenMP_C)
# Tambahkan library eksternal lain (libslink, onnxruntime, sqlite3, mqtt) di sini 