cmake_minimum_required(VERSION 3.7)
project(Cpl)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include_directories(${ROOT_DIR}/src)

file(GLOB_RECURSE TEST_SRC ${ROOT_DIR}/src/Test/*.cpp)
add_executable(Test ${TEST_SRC})

option(CPL_SANITIZE "Build the tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(CPL_SANITIZE)
    # vptr checks are disabled: Param.h walks parameter fields through casts between unrelated types.
    # -O1 is required for stack-use-after-return detection (run with ASAN_OPTIONS=detect_stack_use_after_return=1).
    set(CPL_SANITIZE_FLAGS -fsanitize=address,undefined -fno-sanitize=vptr -fno-sanitize-recover=all -fno-omit-frame-pointer -O1 -g)
    target_compile_options(Test PRIVATE ${CPL_SANITIZE_FLAGS})
    target_link_libraries(Test ${CPL_SANITIZE_FLAGS})
endif()

target_link_libraries(Test -lpthread -lstdc++ -lstdc++fs -lm)

if(UNIX)
target_link_libraries(Test -ldl)
endif()
