include(CheckFunctionExists)
include(FindThreads)

if(!${CMAKE_USE_PTHREADS_INIT})
    message(FATAL_ERROR "Unable to find pthreads")
endif()

add_definitions(-DHAVE_CONFIG_H=1 -D_GNU_SOURCE)

check_function_exists(futimes HAVE_FUTIMES)
check_function_exists(readahead HAVE_READAHEAD)
check_function_exists(fdatasync HAVE_FDATASYNC)
check_function_exists(pread HAVE_PREAD)
check_function_exists(pwrite HAVE_PWRITE)
check_function_exists(sendfile HAVE_SENDFILE)
check_function_exists(sync_file_range HAVE_SYNC_FILE_RANGE)

if(${HAVE_PREAD} AND ${HAVE_PWRITE})
   set(HAVE_PREADWRITE 1)
endif()

configure_file(config.h.cmake ${PROJECT_BINARY_DIR}/deps/libeio/config.h)
include_directories(${PROJECT_BINARY_DIR}/deps/libeio)

add_library(eio eio.c)
target_link_libraries(eio ${CMAKE_THREAD_LIBS_INIT})
