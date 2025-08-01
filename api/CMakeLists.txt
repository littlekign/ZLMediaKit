﻿# MIT License
#
# Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

include(GenerateExportHeader)
file(GLOB API_SRC_LIST
  include/*.h
  source/*.c
  source/*.cpp
  source/*.h)

set(LINK_LIBRARIES ${MK_LINK_LIBRARIES})

set(COMPILE_DEFINITIONS ${MK_COMPILE_DEFINITIONS})

if (MSVC)
  list(APPEND COMPILE_DEFINITIONS GENERATE_EXPORT)
endif ()

if(ENABLE_API_STATIC_LIB)
  add_library(mk_api STATIC ${API_SRC_LIST})
  list(APPEND COMPILE_DEFINITIONS MediaKitApi_STATIC)
elseif(IOS)
  add_library(mk_api STATIC ${API_SRC_LIST})
else()
  add_library(mk_api SHARED ${API_SRC_LIST})
endif()

target_compile_definitions(mk_api
  PRIVATE ${COMPILE_DEFINITIONS})
target_include_directories(mk_api
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(mk_api -Wl,--start-group ${LINK_LIBRARIES} -Wl,--end-group)
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
  target_link_libraries(mk_api log -Wl,--start-group ${LINK_LIBRARIES} -Wl,--end-group)
else()
  target_link_libraries(mk_api ${LINK_LIBRARIES})
endif()

generate_export_header(mk_api
  EXPORT_MACRO_NAME API_EXPORT
  BASE_NAME MK_API
  STATIC_DEFINE MediaKitApi_STATIC
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/mk_export.h")

file(GLOB API_HEADER_LIST include/*.h ${CMAKE_CURRENT_BINARY_DIR}/*.h)
install(FILES ${API_HEADER_LIST}
  DESTINATION ${INSTALL_PATH_INCLUDE})
install(TARGETS mk_api
  ARCHIVE DESTINATION ${INSTALL_PATH_LIB}
  LIBRARY DESTINATION ${INSTALL_PATH_LIB}
  RUNTIME DESTINATION ${INSTALL_PATH_RUNTIME})

if(MSVC)
  set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/resource.rc")
  set_source_files_properties(${RESOURCE_FILE} PROPERTIES LANGUAGE RC)
  target_sources(mk_api PRIVATE ${RESOURCE_FILE})
endif()

#relase 类型时额外输出debug调试信息
if(UNIX)
  if("${CMAKE_BUILD_TYPE_LOWER}" STREQUAL "release")
    find_program(OBJCOPY_FOUND objcopy)
    if (OBJCOPY_FOUND)
      add_custom_command(TARGET mk_api
        POST_BUILD
        COMMAND objcopy --only-keep-debug ${EXECUTABLE_OUTPUT_PATH}/libmk_api.so ${EXECUTABLE_OUTPUT_PATH}/libmk_api.so.debug
        COMMAND objcopy --strip-all ${EXECUTABLE_OUTPUT_PATH}/libmk_api.so
        COMMAND objcopy --add-gnu-debuglink=${EXECUTABLE_OUTPUT_PATH}/libmk_api.so.debug ${EXECUTABLE_OUTPUT_PATH}/libmk_api.so
      )
      install(FILES ${EXECUTABLE_OUTPUT_PATH}/libmk_api.so.debug DESTINATION ${INSTALL_PATH_RUNTIME})
    else()
      message(STATUS "not found objcopy, generate libmk_api.so.debug skip")
    endif()
  endif()
endif()

# IOS 跳过测试代码
if(IOS)
  return()
endif()

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
