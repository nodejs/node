if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  return()
endif()

add_subdirectory(${LIBC_TARGET_OS})
set(target_os_util libc.src.__support.OSUtil.${LIBC_TARGET_OS}.${LIBC_TARGET_OS}_util)
if(NOT TARGET ${target_os_util})
  return()
endif()

add_object_library(
  osutil
  ALIAS
    ${target_os_util}
  DEPENDS
    ${target_os_util}
)
