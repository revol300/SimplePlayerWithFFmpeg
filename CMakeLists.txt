cmake_minimum_required(VERSION 3.10)

project(SimplePlayerWithFFMpeg)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Install git hooks
execute_process(COMMAND /bin/sh -c "cd ${CMAKE_SOURCE_DIR}; git config core.hooksPath hooks")

INCLUDE(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

include_directories(
	${PROJECT_SOURCE_DIR}/demuxer
	${PROJECT_SOURCE_DIR}/decoder
	${PROJECT_SOURCE_DIR}/converter
	${PROJECT_SOURCE_DIR}/renderer
)

add_executable(demuxer demuxer/demuxer_test.cc demuxer/demuxer.cc)
add_executable(decoder decoder/decoder_test.cc demuxer/demuxer.cc decoder/decoder.cc)
add_executable(converter converter/converter_test.cc demuxer/demuxer.cc decoder/decoder.cc converter/audio_converter.cc converter/video_converter.cc)
add_executable(renderer renderer/renderer_test.cc demuxer/demuxer.cc decoder/decoder.cc converter/audio_converter.cc converter/video_converter.cc renderer/video_renderer.cc renderer/audio_renderer.cc timer.cc)
add_executable(player player/player.cc demuxer/demuxer.cc decoder/decoder.cc converter/audio_converter.cc converter/video_converter.cc renderer/video_renderer.cc renderer/audio_renderer.cc timer.cc)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
set(LIBRARIES
  CONAN_PKG::ffmpeg
  CONAN_PKG::sdl2
  wsock32
  ws2_32
  Secur32
)
ELSE()
set(LIBRARIES
  CONAN_PKG::ffmpeg
  CONAN_PKG::sdl2
)
ENDIF()

target_link_libraries(demuxer ${LIBRARIES})
target_link_libraries(decoder ${LIBRARIES})
target_link_libraries(converter ${LIBRARIES})
target_link_libraries(renderer ${LIBRARIES})
target_link_libraries(player ${LIBRARIES})
