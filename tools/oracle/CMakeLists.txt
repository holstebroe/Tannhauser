# Out-of-tree oracle driver (docs/tearwash/04_VALIDATION.md §1). Not part of the plugin build:
# it compiles BlueBox's emulator core from a local checkout given as -DBLUEBOX_DIR=...
# Use tools/oracle/setup_bluebox.sh rather than configuring this by hand.
cmake_minimum_required(VERSION 3.15)
project(tearwash_oracle CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT BLUEBOX_DIR OR NOT EXISTS "${BLUEBOX_DIR}/emu/src/engine.cpp")
    message(FATAL_ERROR "Set -DBLUEBOX_DIR to a BlueBox checkout (see tools/oracle/README.md)")
endif()
set(BB "${BLUEBOX_DIR}/emu/src")
add_library(lexcore STATIC ${BB}/i8080.cpp ${BB}/dsp.cpp ${BB}/machine.cpp ${BB}/romfile.cpp
                           ${BB}/resampler.cpp ${BB}/engine.cpp)
target_include_directories(lexcore PUBLIC ${BB})
add_executable(tearwash_oracle tearwash_oracle.cpp)
target_include_directories(tearwash_oracle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(tearwash_oracle PRIVATE lexcore)
