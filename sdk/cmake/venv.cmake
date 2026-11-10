include_guard(GLOBAL)

# Installs sdk/tools/requirements.txt into a venv under CAPI_DIR the first
# time a build needs it, and sets CAPI_VENV_PYTHON -- so a fresh checkout
# configures with no manual pip step. Module parsing needs it before
# project(), so both callers share this setup.
function(_capi_setup_venv)
    if(CAPI_VENV_PYTHON)
        return()
    endif()
    set(_capi_venv_dir "${CAPI_DIR}/.capi_venv")
    if(CMAKE_HOST_WIN32)
        set(_capi_venv_python "${_capi_venv_dir}/Scripts/python.exe")
    else()
        set(_capi_venv_python "${_capi_venv_dir}/bin/python3")
    endif()

    if(NOT EXISTS "${_capi_venv_python}")
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        message(STATUS "capi: creating Python venv in ${_capi_venv_dir}")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m venv "${_capi_venv_dir}"
            RESULT_VARIABLE _capi_venv_result)
        if(NOT _capi_venv_result EQUAL 0)
            message(FATAL_ERROR "capi: could not create a Python venv in ${_capi_venv_dir}")
        endif()
    endif()

    execute_process(
        COMMAND "${_capi_venv_python}" -c "import kconfiglib, yaml, jsonschema, referencing"
        RESULT_VARIABLE _capi_venv_import_result
        ERROR_QUIET)
    if(NOT _capi_venv_import_result EQUAL 0)
        execute_process(
            COMMAND "${_capi_venv_python}" -m pip install --quiet
                    -r "${CAPI_DIR}/sdk/tools/requirements.txt"
            RESULT_VARIABLE _capi_venv_result)
        if(NOT _capi_venv_result EQUAL 0)
            message(FATAL_ERROR "capi: could not install sdk/tools/requirements.txt")
        endif()
    endif()

    set(CAPI_VENV_PYTHON "${_capi_venv_python}" PARENT_SCOPE)
endfunction()
