check_function_exists(getopt HAVE_GETOPT)

set(TEST_PROGRAMS
  add_from_filep
  can_clone_file
  fopen_unchanged
  fseek
  fuzz_main
  nonrandomopentest
  liboverride-test
)

set(GETOPT_USERS
  fread
  tryopen
)

set(HOLE_USERS
  hole
  ziptool_regress
)

set(ZIP_PROGRAMS ${TEST_PROGRAMS} ${GETOPT_USERS} ${HOLE_USERS})

foreach(PROGRAM IN LISTS ZIP_PROGRAMS)
  add_executable(${PROGRAM} ${PROGRAM}.c)
  target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
  target_link_libraries(${PROGRAM} zip)
endforeach()

# both programs using source_hole.c also use getopt
if(NOT HAVE_GETOPT)
  foreach(PROGRAM IN LISTS GETOPT_USERS HOLE_USERS)
    target_sources(${PROGRAM} PRIVATE ../src/getopt.c)
    target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)
  endforeach()
endif(NOT HAVE_GETOPT)

foreach(PROGRAM IN LISTS HOLE_USERS)
  target_sources(${PROGRAM} PRIVATE source_hole.c)
endforeach()

# for including ziptool.c
target_include_directories(ziptool_regress PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)

set(DL_USERS
  # malloc
  nonrandomopen
  liboverride
)

foreach(PROGRAM IN LISTS DL_USERS)
  add_library(${PROGRAM} MODULE ${PROGRAM}.c)
  target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
endforeach()

add_custom_target(cleanup
    COMMAND ${CMAKE_COMMAND} -DDIR=${PROJECT_BINARY_DIR}/regress -P ${PROJECT_SOURCE_DIR}/regress/cleanup.cmake
    )

add_custom_target(testinput
  ALL
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E tar x ${PROJECT_SOURCE_DIR}/regress/manyfiles-zip.zip
  COMMAND ${CMAKE_COMMAND} -E tar x ${PROJECT_SOURCE_DIR}/regress/bigzero-zip.zip
  DEPENDS ${PROJECT_SOURCE_DIR}/regress/manyfiles-zip.zip ${PROJECT_SOURCE_DIR}/regress/bigzero-zip.zip
)

set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
  bigzero.zip
  manyfiles-133000.zip
  manyfiles-65536.zip
  manyfiles-fewer.zip
  manyfiles-more.zip
  manyfiles-zip64-modulo.zip
  manyfiles-zip64.zip
  manyfiles.zip
)

set(path "$<TARGET_FILE_DIR:zip>;$ENV{PATH}")
if (TARGET zlib)
  set(path "$<TARGET_FILE_DIR:zlib>;${path}")
endif()
string(REPLACE ";" "\\;" path "${path}")

set(ENV{srcdir} ${PROJECT_SOURCE_DIR}/regress)

file(GLOB EXTRA_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/*.test)
foreach(FULL_CASE IN LISTS EXTRA_TESTS)
  get_filename_component(CASE ${FULL_CASE} NAME)
  add_test(NAME ${CASE} COMMAND ${PERL_EXECUTABLE} ${PROJECT_BINARY_DIR}/regress/runtest -v --bin-sub-directory $<CONFIG> ${PROJECT_SOURCE_DIR}/regress/${CASE})
  set_tests_properties(${CASE} PROPERTIES SKIP_RETURN_CODE 77)
  set_tests_properties(${CASE} PROPERTIES ENVIRONMENT "PATH=${path}")
endforeach()

set(XFAIL_TESTS
)

foreach(CASE ${XFAIL_TESTS})
  set_tests_properties(${CASE} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
