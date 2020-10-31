#vim: et ts =4 sts = 4 sw = 4 tw = 0
set(EXAMPLES
    readFromString
    readFromStream
    stringWrite
    streamWrite
)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_definitions(
        -D_SCL_SECURE_NO_WARNINGS
        -D_CRT_SECURE_NO_WARNINGS
        -D_WIN32_WINNT=0x601
        -D_WINSOCK_DEPRECATED_NO_WARNINGS
    )
endif()

foreach(example ${EXAMPLES})
    add_executable(${example} ${example}/${example}.cpp)
    target_include_directories(${example} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${example} jsoncpp_lib)
endforeach()

add_custom_target(examples ALL DEPENDS ${EXAMPLES})
