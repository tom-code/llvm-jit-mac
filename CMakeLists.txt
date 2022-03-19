cmake_minimum_required(VERSION 3.22)
project(llvm-jit-demo)


set (LLVM_DIR /usr/local/opt/llvm/lib/cmake/llvm/)


set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})


add_executable(ljit jit.cc)

llvm_map_components_to_libnames(llvm_libs analysis core executionengine instcombine object orcjit runtimedyld scalaropts support native)
target_link_libraries(ljit ${llvm_libs})

