cmake_minimum_required(VERSION 3.18)

project(targetApp C)

find_path(LWIP_DIR
  NAMES src/Filelists.cmake
  PATHS ENV LWIP_DIR
  DOC "Location of top of LWIP source tree"
  NO_DEFAULT_PATH
)

if(NOT LWIP_DIR)
  message(FATAL "Can not file LWIP")
endif()

set(LWIP_CONTRIB_DIR "${LWIP_DIR}/contrib")

set (LWIP_INCLUDE_DIRS
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "${LWIP_DIR}/src/include"
    "${LWIP_DIR}/contrib"
    "${LWIP_DIR}/contrib/ports/unix/port/include"
)

include(${LWIP_DIR}/src/Filelists.cmake)
#include(${LWIP_DIR}/contrib/Filelists.cmake)
include(${LWIP_DIR}/contrib/ports/unix/Filelists.cmake)

add_executable(
  testpscmsg
  pscapp.c listener.c pscmsg.c
)
target_include_directories(testpscmsg PRIVATE ${LWIP_INCLUDE_DIRS})
target_compile_options(testpscmsg PRIVATE ${LWIP_COMPILER_FLAGS})
target_compile_definitions(testpscmsg PRIVATE ${LWIP_DEFINITIONS})
target_link_libraries(testpscmsg ${LWIP_SANITIZER_LIBS} lwipcontribportunix lwipcore)
