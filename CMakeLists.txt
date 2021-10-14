cmake_minimum_required(VERSION 3.10)

project(SimplePlayerWithFFMpeg)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

INCLUDE(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(demuxer demuxer/demuxer_test.cc demuxer/demuxer.cc)
add_executable(decoder decoder/decoder_test.cc demuxer/demuxer.cc decoder/decoder.cc)
add_executable(converter converter/converter_test.cc demuxer/demuxer.cc decoder/decoder.cc converter/audio_converter.cc converter/video_converter.cc)
add_executable(renderer renderer/renderer_test.cc demuxer/demuxer.cc decoder/decoder.cc converter/audio_converter.cc converter/video_converter.cc renderer/video_renderer.cc renderer/audio_renderer.cc timer.cc)
add_executable(player player/player.cc demuxer/demuxer.cc decoder/decoder.cc converter/audio_converter.cc converter/video_converter.cc renderer/video_renderer.cc renderer/audio_renderer.cc timer.cc)

target_link_libraries(demuxer CONAN_PKG::ffmpeg CONAN_PKG::sdl2)
target_link_libraries(decoder CONAN_PKG::ffmpeg CONAN_PKG::sdl2)
target_link_libraries(converter CONAN_PKG::ffmpeg CONAN_PKG::sdl2)
target_link_libraries(renderer CONAN_PKG::ffmpeg CONAN_PKG::sdl2)
target_link_libraries(player CONAN_PKG::ffmpeg CONAN_PKG::sdl2)
