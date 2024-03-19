#
# Copyright 2024 Bernd Pfrommer <bernd.pfrommer@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22)
project(flex_sync)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_ros REQUIRED)
find_package(ament_cmake_auto REQUIRED)

set(ament_dependencies
  "rclcpp")

foreach(pkg ${ament_dependencies})
  find_package(${pkg} REQUIRED)
endforeach()

#
# --------- library
#
add_library(${PROJECT_NAME} INTERFACE)
add_dependencies(${PROJECT_NAME} rclcpp)
target_include_directories(
    ${PROJECT_NAME}
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

install(TARGETS
  ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_export)

install(DIRECTORY
  include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

ament_export_targets(${PROJECT_NAME}_export HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp)


if(BUILD_TESTING)
  find_package(ament_cmake REQUIRED)
  find_package(ament_cmake_copyright REQUIRED)
  find_package(ament_cmake_cppcheck REQUIRED)
  find_package(ament_cmake_cpplint REQUIRED)
  find_package(ament_cmake_clang_format REQUIRED)
  find_package(ament_cmake_lint_cmake REQUIRED)
  find_package(ament_cmake_xmllint REQUIRED)

  ament_copyright()
  ament_cppcheck(LANGUAGE c++)
  ament_cpplint(FILTERS "-build/include,-runtime/indentation_namespace")
  ament_clang_format(CONFIG_FILE .clang-format)
  ament_lint_cmake()
  ament_xmllint()

  # ----------- setup gtests
  find_package(ament_cmake_gtest REQUIRED)
  find_package(std_msgs REQUIRED)
  set(SYNC_TEST "${PROJECT_NAME}_sync_test")
  ament_add_gtest(${SYNC_TEST}
    test/sync_test.cpp
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test)
  target_include_directories(${SYNC_TEST} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
  target_compile_features(${SYNC_TEST} PRIVATE cxx_std_17)
  ament_target_dependencies(${SYNC_TEST} std_msgs rclcpp)
  target_link_libraries(${SYNC_TEST} ${PROJECT_NAME})
endif()

ament_package()
