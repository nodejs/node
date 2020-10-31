if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12.0)
    # The new Python3 module is much more robust than the previous PythonInterp
    find_package(Python3 COMPONENTS Interpreter)
    # Set variables for backwards compatibility with cmake < 3.12.0
    set(PYTHONINTERP_FOUND ${Python3_Interpreter_FOUND})
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
else()
    set(Python_ADDITIONAL_VERSIONS 3.8)
    find_package(PythonInterp 3)
endif()

add_executable(jsontestrunner_exe
    main.cpp
)

if(BUILD_SHARED_LIBS)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12.0)
        add_compile_definitions( JSON_DLL )
    else()
        add_definitions(-DJSON_DLL)
    endif()
endif()
target_link_libraries(jsontestrunner_exe jsoncpp_lib)

set_target_properties(jsontestrunner_exe PROPERTIES OUTPUT_NAME jsontestrunner_exe)

if(PYTHONINTERP_FOUND)
    # Run end to end parser/writer tests
    set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../test)
    set(RUNJSONTESTS_PATH ${TEST_DIR}/runjsontests.py)

    # Run unit tests in post-build
    # (default cmake workflow hides away the test result into a file, resulting in poor dev workflow?!?)
    add_custom_target(jsoncpp_readerwriter_tests
        "${PYTHON_EXECUTABLE}" -B "${RUNJSONTESTS_PATH}" $<TARGET_FILE:jsontestrunner_exe> "${TEST_DIR}/data"
        DEPENDS jsontestrunner_exe jsoncpp_test
    )
    add_custom_target(jsoncpp_check DEPENDS jsoncpp_readerwriter_tests)

    ## Create tests for dashboard submission, allows easy review of CI results https://my.cdash.org/index.php?project=jsoncpp
    add_test(NAME jsoncpp_readerwriter
        COMMAND "${PYTHON_EXECUTABLE}" -B "${RUNJSONTESTS_PATH}" $<TARGET_FILE:jsontestrunner_exe> "${TEST_DIR}/data"
        WORKING_DIRECTORY "${TEST_DIR}/data"
    )
    add_test(NAME jsoncpp_readerwriter_json_checker
        COMMAND "${PYTHON_EXECUTABLE}" -B "${RUNJSONTESTS_PATH}" --with-json-checker  $<TARGET_FILE:jsontestrunner_exe> "${TEST_DIR}/data"
        WORKING_DIRECTORY "${TEST_DIR}/data"
    )
endif()
