project(ffmpeg_tutorial)
cmake_minimum_required(VERSION 3.13)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#FFMPEG
# set(FFMPEG_INCLUDE_DIR "/usr/local/Cellar/ffmpeg/4.3_1/include")
# set(FFMPEG_LIB_DIR "/usr/local/Cellar/ffmpeg/4.3_1/lib")
# set(AVCODEC_LIBRARY "${FFMPEG_LIB_DIR}/libavcodec.dylib")
# set(AVFORMAT_LIBRARY "${FFMPEG_LIB_DIR}/libavformat.dylib")
# set(AVUTIL_LIBRARY "${FFMPEG_LIB_DIR}/libavutil.dylib")
# set(SWSCALE_LIBRARY "${FFMPEG_LIB_DIR}/libswscale.dylib")
#
#SDL

# includes cmake/FindSDL2.cmake
# set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
#
# find_package(SDL2 REQUIRED)
# include_directories(${SDL2_INCLUDE_DIR})
# find_package(SDL2)
# set(SDL2_INCLUDE_DIR "/usr/local/Cellar/sdl2/2.0.12_1/include")
# set(SDL2_LIBRARY "/usr/local/Cellar/sdl2/2.0.12_1/lib/libSDL2.dylib")

add_executable(screencapture screen_capture.cc)
add_executable(tutorial02 tutorial02.cc)
add_executable(tutorial03 tutorial03.cc)
add_executable(videoplayer video_player.cc)
# message(${FFMPEG_INCLUDE_DIR})
# message(${AVCODEC_LIBRARY})
# message(${AVFORMAT_LIBRARY})
# message(${AVUTIL_LIBRARY})
# message(${SDL2_LIBRARY})

# include_directories(${FFMPEG_INCLUDE_DIR})
# include_directories(${SDL2_INCLUDE_DIR})

set(FFMPEG_LIBS avutil avformat avcodec avdevice avfilter swresample swscale)
target_link_libraries(screencapture ${FFMPEG_LIBS})
target_link_libraries(tutorial02 ${FFMPEG_LIBS} SDL2)
target_link_libraries(tutorial03 ${FFMPEG_LIBS} SDL2)
target_link_libraries(videoplayer ${FFMPEG_LIBS} SDL2)

set(SIMPLE_PLAYER_DIR "../output")
# install(TARGETS screencapture DESTINATION "${SIMPLE_PLAYER_DIR}")
install(TARGETS screencapture tutorial02 tutorial03 videoplayer DESTINATION "${SIMPLE_PLAYER_DIR}")

# include_directories(${FFMPEG_INCLUDE_DIR})
# target_link_libraries(tutorial ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})

# FFMPEG DOES NOT SUPPORT CMAKE!!!!!!!!!!!!!!!!! DO NOT USE find_package
# find_path, find_library set path of FFMPEG to /usr/local/ffmpeg do not use it
