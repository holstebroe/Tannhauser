cmake_minimum_required(VERSION 3.15)
project(Tannhauser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Link-time optimisation in Release builds (as in Acidus).
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported AND CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
if(MSVC)
    add_link_options($<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/clap/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(CORE_SOURCES
    src/core/Params.cpp
    src/core/SynthEngine.cpp
    src/core/Effects.cpp
    src/presets/Presets.cpp
    src/presets/PresetData.cpp
)

set(CLAP_SOURCES
    src/clap/TannhauserClap.cpp
)

set(GUI_SOURCES
    src/gui/Font.cpp
    src/gui/Graphics.cpp
    src/gui/FileDialog.cpp
    src/gui/modern/ModernDraw.cpp
    src/gui/PanelLayout.cpp
    src/gui/PanelRenderer.cpp
    src/gui/GuiWindow.cpp
)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    set(GUI_LIBS ${X11_LIBRARIES})
    include_directories(${X11_INCLUDE_DIR})
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    set(GUI_LIBS ${COCOA_LIBRARY})
elseif(WIN32)
    set(GUI_LIBS gdi32 user32 comdlg32)
endif()

# The GUI and CLAP glue never run in the audio callback: optimise for size.
if(MSVC)
    set_source_files_properties(src/gui/Font.cpp src/gui/FileDialog.cpp src/gui/PanelLayout.cpp
        src/gui/GuiWindow.cpp ${CLAP_SOURCES} src/presets/PresetData.cpp src/presets/Presets.cpp
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:/O1>")
else()
    set_source_files_properties(src/gui/Font.cpp src/gui/FileDialog.cpp src/gui/PanelLayout.cpp
        src/gui/GuiWindow.cpp ${CLAP_SOURCES} src/presets/PresetData.cpp src/presets/Presets.cpp
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-Os>")
endif()

add_library(tannhauser_core STATIC ${CORE_SOURCES})

add_library(tannhauser MODULE ${CLAP_SOURCES} ${GUI_SOURCES})
target_link_libraries(tannhauser PRIVATE tannhauser_core ${GUI_LIBS})
set_target_properties(tannhauser PROPERTIES PREFIX "" SUFFIX ".clap" OUTPUT_NAME "tannhauser")
if(UNIX AND NOT APPLE)
    # Keep the plugin's symbols to itself; only clap_entry is exported.
    set_target_properties(tannhauser PROPERTIES CXX_VISIBILITY_PRESET hidden C_VISIBILITY_PRESET hidden)
endif()

# Copy the built .clap into $CLAPTEST (not part of ALL).
add_custom_target(deploy_clap
    COMMAND ${CMAKE_COMMAND} -DCLAP_FILE=$<TARGET_FILE:tannhauser>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployClap.cmake
    COMMENT "Deploying tannhauser.clap to $CLAPTEST"
    VERBATIM)
add_dependencies(deploy_clap tannhauser)

# DSP verification (docs/spec/07_VALIDATION.md).
add_executable(tannhauser_dsp_test src/tests/dsp_test_main.cpp)
target_link_libraries(tannhauser_dsp_test PRIVATE tannhauser_core)

# Offline renderer: preset + phrase -> WAV, for listening and A/B.
add_executable(tannhauser_render src/tests/render_main.cpp)
target_link_libraries(tannhauser_render PRIVATE tannhauser_core)

# GUI verification: offscreen render + interaction, plugin state round trip.
add_executable(tannhauser_gui_test ${CLAP_SOURCES} ${GUI_SOURCES} src/tests/gui_test_main.cpp)
target_link_libraries(tannhauser_gui_test PRIVATE tannhauser_core ${GUI_LIBS})

enable_testing()
add_test(NAME dsp COMMAND tannhauser_dsp_test)
add_test(NAME gui COMMAND tannhauser_gui_test)
