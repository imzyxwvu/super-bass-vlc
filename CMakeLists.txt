cmake_minimum_required(VERSION 3.16)
project(super_bass)

set(CMAKE_CXX_STANDARD 14)
include_directories(${VLC_SDK_ROOT}/include/vlc/plugins)
link_directories(${VLC_SDK_ROOT}/lib)
add_compile_definitions(MODULE_STRING="super_bass" __PLUGIN__)

add_library(libsuper_bass_plugin SHARED super_bass_plugin.cpp biquad.h primitives.h)
target_link_libraries(libsuper_bass_plugin libvlccore)