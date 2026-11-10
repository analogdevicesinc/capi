# Parse module.yml with Python and the generated JSON schema.
# Keeps the minimal SDK's CAPI_MODULE_* and CAPI_DEP_<NAME>_* contract.

include("${CAPI_DIR}/sdk/cmake/venv.cmake")

# One version constraint against an actual X.Y.Z, in the grammar from
# docs/module-yml.md. Used both below and by resolve_deps.cmake. subject is
# prose naming who required what, so each call site reads as a sentence.
function(_capi_require_version subject constraint actual)
    if(NOT constraint)
        return()
    endif()
    if(NOT actual)
        message(FATAL_ERROR "${subject} ${constraint}, but no version was declared.")
    endif()

    string(REPLACE "," ";" _capi_rv_bounds "${constraint}")
    foreach(_capi_rv_bound IN LISTS _capi_rv_bounds)
        if(NOT _capi_rv_bound MATCHES "^(>=|<=|=|>|<)([0-9]+\\.[0-9]+\\.[0-9]+)$")
            # Backstop: the schema rejects this shape first.
            message(FATAL_ERROR
                "${subject} '${constraint}', which is not a version constraint "
                "(see docs/module-yml.md).")
        endif()
        set(_capi_rv_op "${CMAKE_MATCH_1}")
        set(_capi_rv_want "${CMAKE_MATCH_2}")

        set(_capi_rv_ok FALSE)
        if(_capi_rv_op STREQUAL ">=")
            if("${actual}" VERSION_GREATER_EQUAL "${_capi_rv_want}")
                set(_capi_rv_ok TRUE)
            endif()
        elseif(_capi_rv_op STREQUAL "<=")
            if("${actual}" VERSION_LESS_EQUAL "${_capi_rv_want}")
                set(_capi_rv_ok TRUE)
            endif()
        elseif(_capi_rv_op STREQUAL "=")
            if("${actual}" VERSION_EQUAL "${_capi_rv_want}")
                set(_capi_rv_ok TRUE)
            endif()
        elseif(_capi_rv_op STREQUAL ">")
            if("${actual}" VERSION_GREATER "${_capi_rv_want}")
                set(_capi_rv_ok TRUE)
            endif()
        elseif(_capi_rv_op STREQUAL "<")
            if("${actual}" VERSION_LESS "${_capi_rv_want}")
                set(_capi_rv_ok TRUE)
            endif()
        endif()

        if(NOT _capi_rv_ok)
            message(FATAL_ERROR
                "${subject} ${constraint}, but the version is ${actual}.")
        endif()
    endforeach()
endfunction()

macro(capi_parse_module module_yml)
    if(NOT EXISTS "${module_yml}")
        message(FATAL_ERROR "module.yml not found at ${module_yml}")
    endif()

    _capi_setup_venv()

    set(_capi_parse_old_deps "${CAPI_MODULE_PACKAGES}")
    foreach(_capi_parse_old_platform IN LISTS CAPI_MODULE_PLATFORMS)
        list(APPEND _capi_parse_old_deps "capi-${_capi_parse_old_platform}")
    endforeach()
    foreach(_capi_parse_old_dep IN LISTS _capi_parse_old_deps)
        string(MAKE_C_IDENTIFIER "${_capi_parse_old_dep}" _capi_parse_old_id)
        string(TOUPPER "${_capi_parse_old_id}" _capi_parse_old_id)
        foreach(_capi_parse_field IN ITEMS VERSION PATH GIT CHECKOUT SUBDIR)
            unset(CAPI_DEP_${_capi_parse_old_id}_${_capi_parse_field})
        endforeach()
    endforeach()
    foreach(_capi_parse_field IN ITEMS NAME TYPE VERSION CAPI_VERSION_REQUIREMENT
                                      CMAKE_TOOLCHAIN CMAKE_VENDOR_SDK
                                      PLATFORMS PACKAGES INTERFACES)
        set(CAPI_MODULE_${_capi_parse_field} "")
    endforeach()

    get_filename_component(_capi_parse_absolute "${module_yml}" ABSOLUTE)
    string(MD5 _capi_parse_hash "${_capi_parse_absolute}")
    set(_capi_parse_out "${CMAKE_BINARY_DIR}/capi_resolve/describe_${_capi_parse_hash}.cmake")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/capi_resolve")
    execute_process(
        COMMAND "${CAPI_VENV_PYTHON}" "${CAPI_DIR}/sdk/tools/module_yaml.py"
                describe "${_capi_parse_absolute}" --out "${_capi_parse_out}"
        RESULT_VARIABLE _capi_parse_result
        ERROR_VARIABLE _capi_parse_error)
    if(NOT _capi_parse_result EQUAL 0)
        message(FATAL_ERROR "capi: failed to parse ${module_yml}:\n${_capi_parse_error}")
    endif()
    include("${_capi_parse_out}")

    _capi_require_version("${module_yml} needs capi"
                          "${CAPI_MODULE_CAPI_VERSION_REQUIREMENT}"
                          "${CAPI_OWN_VERSION}")

    if(NOT CMAKE_SCRIPT_MODE_FILE)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                     "${_capi_parse_absolute}"
                     "${CAPI_DIR}/sdk/tools/module_yaml.py"
                     "${CAPI_DIR}/sdk/schemas/module.base.schema.v1.json"
                     "${CAPI_DIR}/sdk/schemas/${CAPI_MODULE_TYPE}.module.schema.v1.json")
    endif()
endmacro()
