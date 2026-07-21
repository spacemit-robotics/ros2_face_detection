# face_detection: face detection node, publishes boxes and optional Detection2DArray.
cmake_minimum_required(VERSION 3.10)
project(face_detection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(vision_msgs QUIET)
if(vision_msgs_FOUND)
  set(HAVE_VISION_MSGS 1)
  message(STATUS "face_detection: vision_msgs found, will publish /perception/faces (Detection2DArray)")
else()
  message(STATUS "face_detection: vision_msgs not found, /perception/faces disabled (install ros-humble-vision-msgs to enable)")
endif()

set(_opencv5_dir "/opt/opencv-spacemit/lib/cmake/opencv5")
set(_opencv4_dir "/opt/opencv-spacemit/lib/cmake/opencv4")
if(NOT DEFINED OpenCV_DIR)
  if(EXISTS "${_opencv5_dir}/OpenCVConfig.cmake")
    set(OpenCV_DIR "${_opencv5_dir}")
  else()
    set(OpenCV_DIR "${_opencv4_dir}")
  endif()
endif()
find_package(OpenCV 5 QUIET CONFIG
  COMPONENTS core imgproc highgui
  NO_DEFAULT_PATH
)
if(NOT OpenCV_FOUND)
  unset(OpenCV_DIR)
  unset(OpenCV_DIR CACHE)
  set(OpenCV_DIR "${_opencv4_dir}")
  find_package(OpenCV 4 REQUIRED CONFIG
    COMPONENTS core imgproc highgui
    NO_DEFAULT_PATH
  )
endif()
if(OpenCV_FOUND)
  message(STATUS "OpenCV found: ${OpenCV_VERSION}")
else()
  message(FATAL_ERROR "OpenCV not found! Set OpenCV_DIR to your opencv5/opencv4 cmake path (e.g. /opt/opencv-spacemit/lib/cmake/opencv5)")
endif()

find_package(yaml-cpp REQUIRED)

set(SPACEMIT_ROBOT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(VISION_SERVICE_INCLUDE_DIR "${SPACEMIT_ROBOT_ROOT}/output/staging/include")

if(NOT EXISTS "${VISION_SERVICE_INCLUDE_DIR}/vision_service.h")
  message(FATAL_ERROR "vision_service.h not found: ${VISION_SERVICE_INCLUDE_DIR}/vision_service.h")
endif()

set(VISION_SERVICE_LIBRARY "${SPACEMIT_ROBOT_ROOT}/output/staging/lib/libvision.so")
if(NOT EXISTS "${VISION_SERVICE_LIBRARY}")
  message(FATAL_ERROR
    "vision library not found at ${VISION_SERVICE_LIBRARY}. "
    "Please build components/model_zoo/vision first.")
endif()

add_executable(face_detection_node
  src/face_detection_node.cpp
  src/detection_utils.cpp
  src/image_utils.cpp
)
target_include_directories(face_detection_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${VISION_SERVICE_INCLUDE_DIR}
)

if(HAVE_VISION_MSGS)
  target_compile_definitions(face_detection_node PRIVATE HAVE_VISION_MSGS=1)
endif()

set(_face_deps ament_index_cpp rclcpp)
ament_target_dependencies(face_detection_node ${_face_deps})

target_link_libraries(face_detection_node
  sensor_msgs::sensor_msgs__rosidl_typesupport_cpp
  std_msgs::std_msgs__rosidl_typesupport_cpp
  ${VISION_SERVICE_LIBRARY}
  ${OpenCV_LIBS}
  yaml-cpp
)
if(vision_msgs_FOUND)
  target_link_libraries(face_detection_node vision_msgs::vision_msgs__rosidl_typesupport_cpp)
endif()

get_filename_component(_opencv_lib_dir "${OpenCV_DIR}/../.." REALPATH)
get_filename_component(_vision_lib_dir "${VISION_SERVICE_LIBRARY}" DIRECTORY)
set_target_properties(face_detection_node PROPERTIES
  INSTALL_RPATH "${_opencv_lib_dir};${_vision_lib_dir}"
  BUILD_WITH_INSTALL_RPATH FALSE
  BUILD_RPATH "${_opencv_lib_dir};${_vision_lib_dir}"
)

install(TARGETS face_detection_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})
ament_package()