# Written in 2017 by Henrik Steffen Gaßmann henrik@gassmann.onl
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

set(TARGET_ARCHITECTURE_TEST_FILE "${CMAKE_CURRENT_LIST_DIR}/../test-arch.c")

function(detect_target_architecture OUTPUT_VARIABLE)
    message(STATUS "${CMAKE_CURRENT_LIST_DIR}")
    try_compile(_IGNORED "${CMAKE_CURRENT_BINARY_DIR}"
        "${TARGET_ARCHITECTURE_TEST_FILE}"
        OUTPUT_VARIABLE _LOG
    )

    string(REGEX MATCH "##arch=([^#]+)##" _IGNORED "${_LOG}")

    set(${OUTPUT_VARIABLE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    set("${OUTPUT_VARIABLE}_${CMAKE_MATCH_1}" 1 PARENT_SCOPE)
    if (CMAKE_MATCH_1 STREQUAL "unknown")
        message(WARNING "could not detect the target architecture.")
    endif()
endfunction()
