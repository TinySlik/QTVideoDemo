cmake_minimum_required(VERSION 3.5)

project(camera_x LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Multimedia Widgets Network MultimediaWidgets REQUIRED)

set(QRC_SOURCE_FILES ${PROJECT_NAME}.qrc)
qt5_add_resources(QRC_FILES ${QRC_SOURCE_FILES})

add_executable(${PROJECT_NAME}
  main.cpp
  camera.cpp
  imagesettings.cpp
  videosettings.cpp
  avcodexmanager.cpp
  camera.ui
  imagesettings.ui
  videosettings.ui
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/ffmpeg-4.2.3-win64-dev/include)
target_link_directories(${PROJECT_NAME} PRIVATE C:/Users/frank/Desktop/develop/camera/ffmpeg-4.2.3-win64-dev/lib)
target_link_libraries(${PROJECT_NAME} PRIVATE avcodec avdevice avfilter avformat avutil postproc swresample swscale
    Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::MultimediaWidgets)
