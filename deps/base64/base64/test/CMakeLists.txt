# Written in 2016 by Henrik Steffen Gaßmann henrik@gassmann.onl
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to the
# public domain worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication
# along with this software. If not, see
#
#     http://creativecommons.org/publicdomain/zero/1.0/
#
########################################################################

function(add_base64_test TEST_NAME)
    unset(SRC_FILE)
    foreach(SRC_FILE ${ARGN})
        list(APPEND SRC_FILES "${SRC_FILE}")
    endforeach()

    add_executable(${TEST_NAME} ${SRC_FILES})
    target_link_libraries(${TEST_NAME} PRIVATE base64)

    add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
    )
    install(TARGETS ${TEST_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()


add_base64_test(test_base64
    codec_supported.c
    test_base64.c
)

add_base64_test(benchmark
    codec_supported.c
    benchmark.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(benchmark PRIVATE rt)
endif()

