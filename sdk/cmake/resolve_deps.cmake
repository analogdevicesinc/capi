# CAPI dependency resolution.
#
# capi_resolve_deps() reads depends.packages of the module being built and
# locates each one, trying in order:
#
#   1. $ENV{<NAME>_DIR}  an explicitly pointed-at checkout. Derived from the
#                        name, not declared: capi-maxim is CAPI_MAXIM_DIR.
#   2. <path>            a checkout on disk. Defaults to //<name>; '//' is
#                        relative to CAPI_WORKSPACE, anything else to the
#                        declaring module, which it may not leave.
#   3. <git>             cloned at <checkout> into that path.
#
# <subdir> then names the package inside what was found. Each resolved
# dependency is published as <NAME>_DIR.
#
# depends.platform is not resolved here: its entries are keyed by PLATFORM
# and capi_resolve_platform() resolves only the selected one.

# A resolved directory must not contain a '"' -- it reaches
# CMAKE_TOOLCHAIN_FILE, which CMake embeds in a quoted include() argument --
# and its module.yml's name: must be dep_name, or an override pointed at the
# wrong checkout is accepted purely because the directory exists.
function(_capi_validate_resolved_dep dep_name resolved_dir constraint)
    string(MAKE_C_IDENTIFIER "${dep_name}" _capi_validate_env)
    string(TOUPPER "${_capi_validate_env}_DIR" _capi_validate_env)
    if(resolved_dir MATCHES "[\";]")
        message(FATAL_ERROR
            "${dep_name} resolved to '${resolved_dir}', which contains a "
            "literal '\"' or ';' character and is unsafe to use here. Point "
            "${dep_name}'s ${_capi_validate_env} or path: at a directory "
            "without "
            "either character.")
    endif()
    set(_capi_validate_module_yml "${resolved_dir}/module.yml")
    if(EXISTS "${_capi_validate_module_yml}")
        capi_parse_module("${_capi_validate_module_yml}")
        _capi_require_version("  ${dep_name}: this module asks for"
                              "${constraint}" "${CAPI_MODULE_VERSION}")
        if(CAPI_MODULE_NAME AND NOT CAPI_MODULE_NAME STREQUAL dep_name)
            message(FATAL_ERROR
                "${dep_name} resolved to '${resolved_dir}', but that "
                "directory's module.yml declares name: ${CAPI_MODULE_NAME}, "
                "not ${dep_name}. Point ${dep_name}'s own override or path: "
                "there instead of ${CAPI_MODULE_NAME}'s checkout.")
        endif()
    endif()
endfunction()

# Sets out_var to one dependency's source directory. A function, so its
# locals stay out of the caller's scope; PARENT_SCOPE still reaches the
# caller of capi_resolve_deps(), which is a macro.
#
# ARGV7 is the directory a relative path: is taken against -- the declaring
# module's own. Defaults to CMAKE_SOURCE_DIR.
function(_capi_resolve_dep dep_name path git checkout subdir constraint out_var)
    set(_capi_dep_base "${CMAKE_SOURCE_DIR}")
    if(ARGC GREATER 7)
        set(_capi_dep_base "${ARGV7}")
    endif()

    string(MAKE_C_IDENTIFIER "${dep_name}" _capi_dep_id)
    string(TOUPPER "${_capi_dep_id}" _capi_dep_id)
    set(_capi_dep_env "${_capi_dep_id}_DIR")

    if(NOT path)
        set(path "//${dep_name}")
    endif()
    # '//' is relative to CAPI_WORKSPACE, anything else to the declaring
    # module's directory. Either way the path may not escape what it is
    # relative to: '..' can be written, but not to leave.
    if(path MATCHES "^//(.*)$")
        if(NOT DEFINED ENV{CAPI_WORKSPACE} OR "$ENV{CAPI_WORKSPACE}" STREQUAL "")
            message(FATAL_ERROR
                "  ${dep_name}: path '${path}' is relative to CAPI_WORKSPACE, "
                "which is not set.")
        endif()
        set(_capi_dep_root "$ENV{CAPI_WORKSPACE}")
        set(_capi_dep_within "CAPI_WORKSPACE (${_capi_dep_root})")
        set(_capi_dep_hint "")
        set(path_rel "${CMAKE_MATCH_1}")
    else()
        set(_capi_dep_root "${_capi_dep_base}")
        set(_capi_dep_within "${_capi_dep_root}")
        set(_capi_dep_hint " Use '//<dir>' for a package outside it.")
        set(path_rel "${path}")
    endif()
    get_filename_component(_capi_dep_root "${_capi_dep_root}" ABSOLUTE)
    get_filename_component(_capi_dep_local "${_capi_dep_root}/${path_rel}" ABSOLUTE)
    file(RELATIVE_PATH _capi_dep_rel "${_capi_dep_root}" "${_capi_dep_local}")
    if(_capi_dep_rel STREQUAL "")
        message(FATAL_ERROR
            "  ${dep_name}: path '${path}' names ${_capi_dep_within} itself, "
            "not a package in it.")
    endif()
    if(_capi_dep_rel MATCHES "^\\.\\.(/|$)")
        message(FATAL_ERROR
            "  ${dep_name}: path '${path}' leaves ${_capi_dep_within}."
            "${_capi_dep_hint}")
    endif()

    if(DEFINED ENV{${_capi_dep_env}} AND NOT IS_DIRECTORY "$ENV{${_capi_dep_env}}")
        message(WARNING
            "${_capi_dep_env}=$ENV{${_capi_dep_env}} is set but is not a "
            "directory -- ignoring it and resolving ${dep_name} from path/git "
            "instead, as if it had never been set.")
    endif()

    if(IS_DIRECTORY "$ENV{${_capi_dep_env}}")
        set(_capi_dep_dir "$ENV{${_capi_dep_env}}")
        message(STATUS "  ${dep_name}: ${_capi_dep_env}=${_capi_dep_dir}")
    elseif(IS_DIRECTORY "${_capi_dep_local}")
        set(_capi_dep_dir "${_capi_dep_local}")
        message(STATUS "  ${dep_name}: ${_capi_dep_dir}")
    elseif(git)
        if(checkout)
            set(_capi_dep_branch --branch "${checkout}")
        else()
            set(_capi_dep_branch "")
        endif()
        message(STATUS "  ${dep_name}: cloning ${git} (${checkout}) into "
                       "${_capi_dep_local}")
        execute_process(
            COMMAND git clone --depth 1 ${_capi_dep_branch} "${git}" "${_capi_dep_local}"
            RESULT_VARIABLE _capi_dep_clone_result)
        if(NOT _capi_dep_clone_result EQUAL 0)
            message(FATAL_ERROR "  ${dep_name}: cloning ${git} failed")
        endif()
        set(_capi_dep_dir "${_capi_dep_local}")
    else()
        message(FATAL_ERROR
            "  ${dep_name}: nowhere to resolve it from. Set ${_capi_dep_env}, "
            "check it out at ${_capi_dep_local}, or give it a git: in "
            "module.yml.")
    endif()

    # subdir: where the package sits inside what was just found.
    if(subdir)
        set(_capi_dep_dir "${_capi_dep_dir}/${subdir}")
        if(NOT IS_DIRECTORY "${_capi_dep_dir}")
            message(FATAL_ERROR
                "  ${dep_name}: subdir '${subdir}' does not exist in the "
                "resolved checkout (${_capi_dep_dir}).")
        endif()
    endif()

    _capi_validate_resolved_dep("${dep_name}" "${_capi_dep_dir}" "${constraint}")

    set(${out_var} "${_capi_dep_dir}" PARENT_SCOPE)
endfunction()

# Resolve one dependency from the CAPI_DEP_<ID>_* variables describe() emits.
# A macro, so out_var lands in the caller's scope.
macro(_capi_resolve_named dep_name base out_var)
    string(MAKE_C_IDENTIFIER "${dep_name}" _capi_rn_id)
    string(TOUPPER "${_capi_rn_id}" _capi_rn_id)
    _capi_resolve_dep("${dep_name}"
        "${CAPI_DEP_${_capi_rn_id}_PATH}"
        "${CAPI_DEP_${_capi_rn_id}_GIT}"
        "${CAPI_DEP_${_capi_rn_id}_CHECKOUT}"
        "${CAPI_DEP_${_capi_rn_id}_SUBDIR}"
        "${CAPI_DEP_${_capi_rn_id}_VERSION}"
        ${out_var}
        "${base}")
endmacro()

macro(capi_resolve_deps)
    capi_parse_module("${CMAKE_SOURCE_DIR}/module.yml")
    message(STATUS "Module: ${CAPI_MODULE_NAME} v${CAPI_MODULE_VERSION} (${CAPI_MODULE_TYPE})")

    foreach(_capi_resolve_dep_name IN LISTS CAPI_MODULE_PACKAGES)
        string(MAKE_C_IDENTIFIER "${_capi_resolve_dep_name}" _capi_resolve_dep_id)
        string(TOUPPER "${_capi_resolve_dep_id}" _capi_resolve_dep_id)

        if(DEFINED _capi_resolve_seen_${_capi_resolve_dep_id}
                AND NOT _capi_resolve_seen_${_capi_resolve_dep_id} STREQUAL _capi_resolve_dep_name)
            message(FATAL_ERROR
                "  ${_capi_resolve_dep_name}: name collides with dependency "
                "\"${_capi_resolve_seen_${_capi_resolve_dep_id}}\" -- both sanitize to "
                "${_capi_resolve_dep_id}. Rename one of them in module.yml.")
        endif()
        set(_capi_resolve_seen_${_capi_resolve_dep_id} "${_capi_resolve_dep_name}")

        _capi_resolve_named("${_capi_resolve_dep_name}" "${CMAKE_SOURCE_DIR}"
                            ${_capi_resolve_dep_id}_DIR)
    endforeach()
endmacro()
