cmake_minimum_required(VERSION 2.8.3)
project(hebi_rosie_demo)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O2")

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  roslib
  std_msgs
  sensor_msgs
  urdf
  message_generation
  genmsg
  actionlib_msgs
  actionlib
  geometry_msgs
  cv_bridge
  hebi_cpp_api
  hebi_cpp_api_examples
)

find_package(
  OpenCV REQUIRED
)

## System dependencies are found with CMake's conventions
find_package(Boost REQUIRED COMPONENTS system)

################################################
## Declare ROS messages, services and actions ##
################################################

## Generate services in the 'srv' folder
add_service_files(
  FILES
  GripperSrv.srv
  VisionSrv.srv
  CalibrateSrv.srv
)

## Generate added messages and services with any dependencies listed here
generate_messages(
  DEPENDENCIES
  std_msgs
  geometry_msgs
  sensor_msgs
  actionlib_msgs
)

###################################
## catkin specific configuration ##
###################################
catkin_package(
#  INCLUDE_DIRS include
#  CATKIN_DEPENDS other_catkin_pkg
#  DEPENDS system_lib
  CATKIN_DEPENDS roscpp
  CATKIN_DEPENDS actionlib
  CATKIN_DEPENDS message_runtime
  CATKIN_DEPENDS hebi_cpp_api
  CATKIN_DEPENDS hebi_cpp_api_examples
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  #include/util
  #include/gains
  #include/librealsense2
  #include/librealsense2/examples
)

add_executable(demo_central src/demo_central.cpp)
add_dependencies(demo_central ${catkin_EXPORTED_TARGETS})
target_link_libraries(demo_central ${catkin_LIBRARIES})

add_executable(gripper_node src/gripper_node.cpp)
add_dependencies(gripper_node ${catkin_EXPORTED_TARGETS})
target_link_libraries(gripper_node ${catkin_LIBRARIES})

add_executable(vision_process src/vision_process.cpp)
add_dependencies(vision_process ${catkin_EXPORTED_TARGETS})
target_link_libraries(vision_process ${catkin_LIBRARIES} ${OpenCV_LIBS})

add_executable(vision_threshold src/vision_threshold.cpp)
add_dependencies(vision_threshold ${catkin_EXPORTED_TARGETS})
target_link_libraries(vision_threshold ${catkin_LIBRARIES} ${OpenCV_LIBS})

#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark executable scripts (Python etc.) for installation
## in contrast to setup.py, you can choose the destination
# install(PROGRAMS
#   scripts/my_python_script
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark executables and/or libraries for installation
# install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_node
#   ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#   RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark cpp header files for installation
# install(DIRECTORY include/${PROJECT_NAME}/
#   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#   FILES_MATCHING PATTERN "*.h"
#   PATTERN ".svn" EXCLUDE
# )

## Mark other files for installation (e.g. launch and bag files, etc.)
install(FILES
  launch/ROSie.launch
  gains/rosie_arm_gains.xml
  gains/rosie_gripper_gains.xml
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)

## I don't think this is the _right_ way to do this, but it works...
#file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/Rosie_Demo/rosie_arm_gains.xml
  #      DESTINATION ${CATKIN_DEVEL_PREFIX}/${CATKIN_PACKAGE_BIN_DESTINATION})
    #file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/Rosie_Demo/rosie_base_gains.xml
  #      DESTINATION ${CATKIN_DEVEL_PREFIX}/${CATKIN_PACKAGE_BIN_DESTINATION})
    #file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/Rosie_Demo/rosie_gripper_gains.xml
  #      DESTINATION ${CATKIN_DEVEL_PREFIX}/${CATKIN_PACKAGE_BIN_DESTINATION})
