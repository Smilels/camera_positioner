cmake_minimum_required(VERSION 2.8.3)
project(camera_positioner)

find_package(catkin REQUIRED COMPONENTS
  apriltags_ros
  roscpp
  tf
)

catkin_package()

include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME} src/${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})
add_dependencies(${PROJECT_NAME} ${catkin_EXPORTED_TARGETS})

add_executable(camera_positioner_table src/camera_positioner_table.cpp)
target_link_libraries(camera_positioner_table ${catkin_LIBRARIES})
add_dependencies(camera_positioner_table ${catkin_EXPORTED_TARGETS})

install(TARGETS ${PROJECT_NAME} DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
install(DIRECTORY launch DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})
