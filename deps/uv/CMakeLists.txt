# TODO: determine CMAKE_SYSTEM_NAME on OS/390.  Currently assumes "OS/390".
cmake_minimum_required(VERSION 3.0)
project(libuv)
enable_testing()

if(MSVC)
  list(APPEND uv_cflags /W4)
elseif(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
  list(APPEND uv_cflags -fvisibility=hidden --std=gnu89)
  list(APPEND uv_cflags -Wall -Wextra -Wstrict-prototypes)
  list(APPEND uv_cflags -Wno-unused-parameter)
endif()

set(uv_sources
    src/fs-poll.c
    src/inet.c
    src/threadpool.c
    src/timer.c
    src/uv-common.c
    src/uv-data-getter-setters.c
    src/version.c)

set(uv_test_sources
    test/blackhole-server.c
    test/echo-server.c
    test/run-tests.c
    test/runner.c
    test/test-active.c
    test/test-async-null-cb.c
    test/test-async.c
    test/test-barrier.c
    test/test-callback-order.c
    test/test-callback-stack.c
    test/test-close-fd.c
    test/test-close-order.c
    test/test-condvar.c
    test/test-connect-unspecified.c
    test/test-connection-fail.c
    test/test-cwd-and-chdir.c
    test/test-default-loop-close.c
    test/test-delayed-accept.c
    test/test-dlerror.c
    test/test-eintr-handling.c
    test/test-embed.c
    test/test-emfile.c
    test/test-env-vars.c
    test/test-error.c
    test/test-fail-always.c
    test/test-fork.c
    test/test-fs-copyfile.c
    test/test-fs-event.c
    test/test-fs-poll.c
    test/test-fs.c
    test/test-get-currentexe.c
    test/test-get-loadavg.c
    test/test-get-memory.c
    test/test-get-passwd.c
    test/test-getaddrinfo.c
    test/test-gethostname.c
    test/test-getnameinfo.c
    test/test-getsockname.c
    test/test-getters-setters.c
    test/test-handle-fileno.c
    test/test-homedir.c
    test/test-hrtime.c
    test/test-idle.c
    test/test-ip4-addr.c
    test/test-ip6-addr.c
    test/test-ip6-addr.c
    test/test-ipc-heavy-traffic-deadlock-bug.c
    test/test-ipc-send-recv.c
    test/test-ipc.c
    test/test-loop-alive.c
    test/test-loop-close.c
    test/test-loop-configure.c
    test/test-loop-handles.c
    test/test-loop-stop.c
    test/test-loop-time.c
    test/test-multiple-listen.c
    test/test-mutexes.c
    test/test-osx-select.c
    test/test-pass-always.c
    test/test-ping-pong.c
    test/test-pipe-bind-error.c
    test/test-pipe-close-stdout-read-stdin.c
    test/test-pipe-connect-error.c
    test/test-pipe-connect-multiple.c
    test/test-pipe-connect-prepare.c
    test/test-pipe-getsockname.c
    test/test-pipe-pending-instances.c
    test/test-pipe-sendmsg.c
    test/test-pipe-server-close.c
    test/test-pipe-set-fchmod.c
    test/test-pipe-set-non-blocking.c
    test/test-platform-output.c
    test/test-poll-close-doesnt-corrupt-stack.c
    test/test-poll-close.c
    test/test-poll-closesocket.c
    test/test-poll-oob.c
    test/test-poll.c
    test/test-process-priority.c
    test/test-process-title-threadsafe.c
    test/test-process-title.c
    test/test-queue-foreach-delete.c
    test/test-ref.c
    test/test-run-nowait.c
    test/test-run-once.c
    test/test-semaphore.c
    test/test-shutdown-close.c
    test/test-shutdown-eof.c
    test/test-shutdown-twice.c
    test/test-signal-multiple-loops.c
    test/test-signal.c
    test/test-socket-buffer-size.c
    test/test-spawn.c
    test/test-stdio-over-pipes.c
    test/test-tcp-alloc-cb-fail.c
    test/test-tcp-bind-error.c
    test/test-tcp-bind6-error.c
    test/test-tcp-close-accept.c
    test/test-tcp-close-while-connecting.c
    test/test-tcp-close.c
    test/test-tcp-connect-error-after-write.c
    test/test-tcp-connect-error.c
    test/test-tcp-connect-timeout.c
    test/test-tcp-connect6-error.c
    test/test-tcp-create-socket-early.c
    test/test-tcp-flags.c
    test/test-tcp-oob.c
    test/test-tcp-open.c
    test/test-tcp-read-stop.c
    test/test-tcp-shutdown-after-write.c
    test/test-tcp-try-write.c
    test/test-tcp-unexpected-read.c
    test/test-tcp-write-after-connect.c
    test/test-tcp-write-fail.c
    test/test-tcp-write-queue-order.c
    test/test-tcp-write-to-half-open-connection.c
    test/test-tcp-writealot.c
    test/test-thread-equal.c
    test/test-thread.c
    test/test-threadpool-cancel.c
    test/test-threadpool.c
    test/test-timer-again.c
    test/test-timer-from-check.c
    test/test-timer.c
    test/test-tmpdir.c
    test/test-tty.c
    test/test-udp-alloc-cb-fail.c
    test/test-udp-bind.c
    test/test-udp-create-socket-early.c
    test/test-udp-dgram-too-big.c
    test/test-udp-ipv6.c
    test/test-udp-multicast-interface.c
    test/test-udp-multicast-interface6.c
    test/test-udp-multicast-join.c
    test/test-udp-multicast-join6.c
    test/test-udp-multicast-ttl.c
    test/test-udp-open.c
    test/test-udp-options.c
    test/test-udp-send-and-recv.c
    test/test-udp-send-hang-loop.c
    test/test-udp-send-immediate.c
    test/test-udp-send-unreachable.c
    test/test-udp-try-send.c
    test/test-walk-handles.c
    test/test-watcher-cross-stop.c)

if(WIN32)
  list(APPEND uv_defines WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)
  list(APPEND uv_libraries
       advapi32
       iphlpapi
       psapi
       shell32
       user32
       userenv
       ws2_32)
  list(APPEND uv_sources
       src/win/async.c
       src/win/core.c
       src/win/detect-wakeup.c
       src/win/dl.c
       src/win/error.c
       src/win/fs.c
       src/win/fs-event.c
       src/win/getaddrinfo.c
       src/win/getnameinfo.c
       src/win/handle.c
       src/win/loop-watcher.c
       src/win/pipe.c
       src/win/thread.c
       src/win/poll.c
       src/win/process.c
       src/win/process-stdio.c
       src/win/signal.c
       src/win/snprintf.c
       src/win/stream.c
       src/win/tcp.c
       src/win/tty.c
       src/win/udp.c
       src/win/util.c
       src/win/winapi.c
       src/win/winsock.c)
  list(APPEND uv_test_libraries ws2_32)
  list(APPEND uv_test_sources src/win/snprintf.c test/runner-win.c)
else()
  list(APPEND uv_defines _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
  list(APPEND uv_libraries pthread)
  list(APPEND uv_sources
       src/unix/async.c
       src/unix/core.c
       src/unix/dl.c
       src/unix/fs.c
       src/unix/getaddrinfo.c
       src/unix/getnameinfo.c
       src/unix/loop-watcher.c
       src/unix/loop.c
       src/unix/pipe.c
       src/unix/poll.c
       src/unix/process.c
       src/unix/signal.c
       src/unix/stream.c
       src/unix/tcp.c
       src/unix/thread.c
       src/unix/tty.c
       src/unix/udp.c)
  list(APPEND uv_test_sources test/runner-unix.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "AIX")
  list(APPEND uv_defines
       _ALL_SOURCE
       _LINUX_SOURCE_COMPAT
       _THREAD_SAFE
       _XOPEN_SOURCE=500)
  list(APPEND uv_libraries perfstat)
  list(APPEND uv_sources src/unix/aix.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  list(APPEND uv_libs dl)
  list(APPEND uv_sources
       src/unix/android-ifaddrs.c
       src/unix/linux-core.c
       src/unix/linux-inotify.c
       src/unix/linux-syscalls.c
       src/unix/procfs-exepath.c
       src/unix/pthread-fixes.c
       src/unix/sysinfo-loadavg.c
       src/unix/sysinfo-memory.c)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Android|Darwin|Linux|OS/390")
  list(APPEND uv_sources src/unix/proctitle.c)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD")
  list(APPEND uv_sources src/unix/freebsd.c)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
  list(APPEND uv_sources src/unix/posix-hrtime.c)
  list(APPEND uv_libraries kvm)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin|DragonFly|FreeBSD|NetBSD|OpenBSD")
  list(APPEND uv_sources src/unix/bsd-ifaddrs.c src/unix/kqueue.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND uv_defines _DARWIN_UNLIMITED_SELECT=1 _DARWIN_USE_64_BIT_INODE=1)
  list(APPEND uv_sources
       src/unix/darwin-proctitle.c
       src/unix/darwin.c
       src/unix/fsevents.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND uv_defines _GNU_SOURCE _POSIX_C_SOURCE=200112)
  list(APPEND uv_libraries dl rt)
  list(APPEND uv_sources
       src/unix/linux-core.c
       src/unix/linux-inotify.c
       src/unix/linux-syscalls.c
       src/unix/procfs-exepath.c
       src/unix/sysinfo-loadavg.c
       src/unix/sysinfo-memory.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  list(APPEND uv_sources src/unix/netbsd.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  list(APPEND uv_sources src/unix/openbsd.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OS/390")
  list(APPEND uv_defines PATH_MAX=255)
  list(APPEND uv_defines _AE_BIMODAL)
  list(APPEND uv_defines _ALL_SOURCE)
  list(APPEND uv_defines _LARGE_TIME_API)
  list(APPEND uv_defines _OPEN_MSGQ_EXT)
  list(APPEND uv_defines _OPEN_SYS_FILE_EXT)
  list(APPEND uv_defines _OPEN_SYS_IF_EXT)
  list(APPEND uv_defines _OPEN_SYS_SOCK_IPV6)
  list(APPEND uv_defines _UNIX03_SOURCE)
  list(APPEND uv_defines _UNIX03_THREADS)
  list(APPEND uv_defines _UNIX03_WITHDRAWN)
  list(APPEND uv_defines _XOPEN_SOURCE_EXTENDED)
  list(APPEND uv_sources
       src/unix/pthread-fixes.c
       src/unix/pthread-barrier.c
       src/unix/os390.c
       src/unix/os390-syscalls.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  list(APPEND uv_defines __EXTENSIONS__ _XOPEN_SOURCE=500)
  list(APPEND uv_libraries kstat nsl sendfile socket)
  list(APPEND uv_sources src/unix/no-proctitle.c src/unix/sunos.c)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin|DragonFly|FreeBSD|Linux|NetBSD|OpenBSD")
  list(APPEND uv_test_libraries util)
endif()

add_library(uv SHARED ${uv_sources})
target_compile_definitions(uv PRIVATE ${uv_defines} BUILDING_UV_SHARED=1)
target_compile_options(uv PRIVATE ${uv_cflags})
target_include_directories(uv PRIVATE include src)
target_link_libraries(uv ${uv_libraries})

add_library(uv_a STATIC ${uv_sources})
target_compile_definitions(uv_a PRIVATE ${uv_defines})
target_compile_options(uv_a PRIVATE ${uv_cflags})
target_include_directories(uv_a PRIVATE include src)
target_link_libraries(uv_a ${uv_libraries})

if(BUILD_TESTING)
  include(CTest)
  add_executable(uv_run_tests ${uv_test_sources})
  target_compile_definitions(uv_run_tests PRIVATE ${uv_defines})
  target_compile_options(uv_run_tests PRIVATE ${uv_cflags})
  target_include_directories(uv_run_tests PRIVATE include)
  target_link_libraries(uv_run_tests uv ${uv_test_libraries})
  add_test(NAME uv_test
           COMMAND uv_run_tests
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_executable(uv_run_tests_a ${uv_test_sources})
  target_compile_definitions(uv_run_tests_a PRIVATE ${uv_defines})
  target_compile_options(uv_run_tests_a PRIVATE ${uv_cflags})
  target_include_directories(uv_run_tests_a PRIVATE include)
  target_link_libraries(uv_run_tests_a uv_a ${uv_test_libraries})
  add_test(NAME uv_test_a
           COMMAND uv_run_tests_a
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(UNIX)
  # Now for some gibbering horrors from beyond the stars...
  include(GNUInstallDirs)
  foreach(x ${uv_libraries})
    set(LIBS "${LIBS} -l${x}")
  endforeach(x)
  file(STRINGS configure.ac configure_ac REGEX ^AC_INIT)
  string(REGEX MATCH [0-9]+[.][0-9]+[.][0-9]+ PACKAGE_VERSION "${configure_ac}")
  set(includedir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
  set(libdir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
  set(prefix ${CMAKE_INSTALL_PREFIX})
  configure_file(libuv.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libuv.pc @ONLY)

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
  install(FILES LICENSE ${CMAKE_CURRENT_BINARY_DIR}/libuv.pc
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  install(TARGETS uv LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(TARGETS uv_a ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
