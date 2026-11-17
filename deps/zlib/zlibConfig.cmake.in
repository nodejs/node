set(_ZLIB_supported_components "shared" "static")

if(ZLIB_FIND_COMPONENTS)
    foreach(_comp ${ZLIB_FIND_COMPONENTS})
        if(NOT _comp IN_LIST _ZLIB_supported_components)
            set(ZLIB_FOUND False)
            set(ZLIB_NOT_FOUND_MESSAGE "Unsupported component: ${_comp}")
        endif(NOT _comp IN_LIST _ZLIB_supported_components)

        include("${CMAKE_CURRENT_LIST_DIR}/ZLIB-${_comp}.cmake"
                OPTIONAL
                RESULT_VARIABLE _comp_loaded)

        if(NOT _comp_loaded)
            set(ZLIB_FOUND False)
            set(ZLIB_NOT_FOUND_MESSAGE "Component ${_comp} not found.")
        else(NOT _comp_loaded)
            set(ZLIB_${_comp}_FOUND TRUE)
        endif(NOT _comp_loaded)
    endforeach(_comp ${ZLIB_FIND_COMPONENTS})
else(ZLIB_FIND_COMPONENTS)
    foreach(_component_config IN LISTS _ZLIB_supported_components)
        include("${CMAKE_CURRENT_LIST_DIR}/ZLIB-${_component_config}.cmake"
                OPTIONAL)
    endforeach(_component_config IN LISTS _ZLIB_supported_components)

    if(NOT TARGET ZLIB::ZLIB)
        set(ZLIB_FOUND False)
        set(ZLIB_NOT_FOUND_MESSAGE "Target ZLIB::ZLIB not created\n")
        string(APPEND ZLIB_NOT_FOUND_MESSAGE "build zlib with support for shared libs or\n")
        string(APPEND ZLIB_NOT_FOUND_MESSAGE "specify COMPONENTS static in your find_package call")
    endif(NOT TARGET ZLIB::ZLIB)

    if(NOT TARGET ZLIB::ZLIBSTATIC)
        set(ZLIB_FOUND False)
        set(ZLIB_NOT_FOUND_MESSAGE "Target ZLIB::ZLIBSTATIC not created\n")
        string(APPEND ZLIB_NOT_FOUND_MESSAGE "build zlib with support for static libs or\n")
        string(APPEND ZLIB_NOT_FOUND_MESSAGE "specify COMPONENTS shared in your find_package call")
    endif(NOT TARGET ZLIB::ZLIBSTATIC)
endif(ZLIB_FIND_COMPONENTS)
