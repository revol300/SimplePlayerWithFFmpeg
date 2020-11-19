project(ffmpeg_tutorial)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  #FFMPEG
  # NOTE : On MacOS, brew install ffmpeg does not install pc file
  # Therefore, write path to ffmpeg library manually
  set(FFMPEG_INCLUDE_DIR "/usr/local/Cellar/ffmpeg/4.3.1_3/include")
  set(FFMPEG_LIB_DIR "/usr/local/Cellar/ffmpeg/4.3.1_3/lib")
  set(AVCODEC_LIBRARY "${FFMPEG_LIB_DIR}/libavcodec.dylib")
  set(AVFORMAT_LIBRARY "${FFMPEG_LIB_DIR}/libavformat.dylib")
  set(AVUTIL_LIBRARY "${FFMPEG_LIB_DIR}/libavutil.dylib")
  set(SWSCALE_LIBRARY "${FFMPEG_LIB_DIR}/libswscale.dylib")
  set(RESAMPLE_LIBRARY "${FFMPEG_LIB_DIR}/libswresample.dylib")
  set(FFMPEG_LIBS ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${RESAMPLE_LIBRARY})


  SET(OperatingSystem "Mac OS X")
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  # # find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
  # FFMPEG DOES NOT SUPPORT CMAKE!!!!!!!!!!!!!!!!! DO NOT USE find_package
  # find_path, find_library set path of FFMPEG to /usr/local/ffmpeg do not use it

  # To find and use ffmpeg avcodec library
  find_path( AVCODEC_INCLUDE_DIR libavcodec/avcodec.h )
  find_library(AVCODEC_LIBRARY avcodec )

  find_path( AVFORMAT_INCLUDE_DIR libavformat/avformat.h )
  find_library( AVFORMAT_LIBRARY avformat )

  find_path( AVUTIL_INCLUDE_DIR libavutil/avutil.h )
  find_library( AVUTIL_LIBRARY avutil )

  find_path( SWSCALE_INCLUDE_DIR libswscale/swscale.h )
  find_library( SWSCALE_LIBRARY swscale )

  find_path( RESAMPLE_INCLUDE_DIR libswresample/swresample.h )
  find_library( RESAMPLE_LIBRARY swresample )

  set(FFMPEG_INCLUDE_DIR ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${SWSCALE_INCLUDE_DIR} ${RESAMPLE_INCLUDE_DIR})
  set(FFMPEG_LIBS ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${RESAMPLE_LIBRARY})
  SET(OperatingSystem "Linux")
ELSE()
  SET(OperatingSystem "Others")
ENDIF()

include_directories(${FFMPEG_INCLUDE_DIR})

find_package(SDL2 REQUIRED)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  include_directories(${SDL2_INCLUDE_DIRS})
ELSE()
  include_directories(${SDL2_INCLUDE_DIR})
ENDIF()

add_executable(screencapture screen_capture.cc)
add_executable(tutorial02 tutorial02.cc)
add_executable(tutorial03 tutorial03.cc)
add_executable(multimediaplayer multimedia_player.cc video_player.cc audio_player.cc)

target_link_libraries(screencapture ${FFMPEG_LIBS})
target_link_libraries(tutorial02 ${FFMPEG_LIBS} ${SDL2_LIBRARIES})
target_link_libraries(tutorial03 ${FFMPEG_LIBS} ${SDL2_LIBRARIES})
target_link_libraries(multimediaplayer ${FFMPEG_LIBS} ${SDL2_LIBRARIES})

set(SIMPLE_PLAYER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../output")
install(TARGETS screencapture tutorial02 tutorial03 multimediaplayer DESTINATION "${SIMPLE_PLAYER_DIR}")
