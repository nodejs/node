if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  return()
endif()
add_subdirectory(${LIBC_TARGET_OS})

set(target_os_util libc.src.__support.OSUtil.${LIBC_TARGET_OS}.${LIBC_TARGET_OS}_util)
if(TARGET ${target_os_util})
  add_object_library(
    osutil
    ALIAS
      ${target_os_util}
    DEPENDS
      ${target_os_util}
  )
endif()

set(target_path libc.src.__support.OSUtil.${LIBC_TARGET_OS}.path)
if(TARGET ${target_path})
  add_header_library(
    path
    HDRS
      path.h
    DEPENDS
      ${target_path}
  )
endif()
