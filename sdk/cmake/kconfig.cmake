# capi_generate_config(<fragment>...) builds one Kconfig tree from every
# path in CAPI_KCONFIG_SOURCES, applies the fragments in order, and brings
# the CONFIG_* variables into the caller's scope, where a platform's
# CMakeLists.txt switches its optional sources on them. kconfiglib does the
# resolving, via sdk/tools/generate_config.py.

include("${CAPI_DIR}/sdk/cmake/venv.cmake")

macro(capi_generate_config)
    set(_capi_kconfig_dir "${CMAKE_BINARY_DIR}/kconfig")
    file(MAKE_DIRECTORY "${_capi_kconfig_dir}")

    # The unified tree is one `source` line per contributing module.
    set(_capi_kconfig_content "")
    foreach(_capi_kconfig_source IN LISTS CAPI_KCONFIG_SOURCES)
        string(APPEND _capi_kconfig_content "source \"${_capi_kconfig_source}\"\n")
    endforeach()
    file(WRITE "${_capi_kconfig_dir}/Kconfig" "${_capi_kconfig_content}")

    # A missing fragment is not an error: an application with no
    # project.conf takes the board's defaults.
    set(_capi_kconfig_fragments "")
    foreach(_capi_kconfig_fragment IN ITEMS ${ARGV})
        if(EXISTS "${_capi_kconfig_fragment}")
            list(APPEND _capi_kconfig_fragments "${_capi_kconfig_fragment}")
        endif()
    endforeach()

    _capi_setup_venv()
    execute_process(
        COMMAND "${CAPI_VENV_PYTHON}" "${CAPI_DIR}/sdk/tools/generate_config.py"
                --kconfig "${_capi_kconfig_dir}/Kconfig"
                --out "${_capi_kconfig_dir}/config.cmake"
                --fragment ${_capi_kconfig_fragments}
        RESULT_VARIABLE _capi_kconfig_result
        OUTPUT_VARIABLE _capi_kconfig_output
        ERROR_VARIABLE _capi_kconfig_error)
    if(NOT _capi_kconfig_result EQUAL 0)
        message(FATAL_ERROR
            "Kconfig generation failed:\n${_capi_kconfig_output}${_capi_kconfig_error}")
    endif()

    include("${_capi_kconfig_dir}/config.cmake")
endmacro()
