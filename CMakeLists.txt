# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(scan)

set(PARTITION_NAME "internal_data")
set(IMAGE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/partition_table/${PARTITION_NAME}.bin")
idf_build_set_property(FLASH_IMAGE ${PARTITION_NAME} ${IMAGE_OUTPUT_PATH})