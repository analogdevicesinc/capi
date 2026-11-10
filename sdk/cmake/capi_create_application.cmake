# Entry point for a CAPI application's CMakeLists.txt:
#
#   include("${CAPI_DIR}/sdk/cmake/capi_create_application.cmake")
#   capi_resolve_platform()
#   project(<name> LANGUAGES C ASM)
#   capi_create_application(<name> SOURCES <files...>)
#
# Split around project() because CMake scans the top-level CMakeLists.txt
# for a literal project() call: inside a macro it runs an implicit
# host-toolchain project() first, caching the wrong compiler before
# CMAKE_TOOLCHAIN_FILE takes effect. Macros, not functions, so what they
# set lands in the application's own scope.
#
# PLATFORM and BOARD come from the active preset. Nothing here names a
# platform: its module.yml supplies the CMake files, and its functions are
# found by convention (capi_<platform>_config_sdk, _add_flash_target).
#
# See docs/ for what a platform package may rely on from here.

include("${CAPI_DIR}/sdk/cmake/module_parser.cmake")
include("${CAPI_DIR}/sdk/cmake/resolve_deps.cmake")
include("${CAPI_DIR}/sdk/cmake/kconfig.cmake")

macro(capi_resolve_platform)
    foreach(_capi_app_required IN ITEMS PLATFORM BOARD)
        if(NOT DEFINED ${_capi_app_required} OR "${${_capi_app_required}}" STREQUAL "")
            message(FATAL_ERROR
                "${_capi_app_required} not set. Use one of the "
                "CMakePresets.json presets.")
        endif()
    endforeach()

    capi_resolve_deps()

    # One platform per build: depends.platform is keyed by PLATFORM, and
    # only the selected entry is resolved.
    string(TOUPPER "${PLATFORM}" _capi_app_platform_upper)
    if(NOT "${PLATFORM}" IN_LIST CAPI_MODULE_PLATFORMS)
        message(FATAL_ERROR
            "PLATFORM='${PLATFORM}' is not a key of depends.platform: in "
            "${CMAKE_SOURCE_DIR}/module.yml, which lists "
            "'${CAPI_MODULE_PLATFORMS}'.")
    endif()
    _capi_resolve_named("capi-${PLATFORM}" "${CMAKE_SOURCE_DIR}"
                        CAPI_${_capi_app_platform_upper}_DIR)
    set(_capi_app_platform_dir "${CAPI_${_capi_app_platform_upper}_DIR}")

    capi_parse_module("${_capi_app_platform_dir}/module.yml")
    set(_capi_app_vendor_sdk "${CAPI_MODULE_CMAKE_VENDOR_SDK}")
    set(_capi_app_toolchain "")
    if(CAPI_MODULE_CMAKE_TOOLCHAIN)
        set(_capi_app_toolchain "${_capi_app_platform_dir}/${CAPI_MODULE_CMAKE_TOOLCHAIN}")
    endif()

    # The platform's own depends:, resolved relative to its directory --
    # that is where its module.yml was written. Anything the application
    # already resolved keeps the application's answer.
    foreach(_capi_app_pdep IN LISTS CAPI_MODULE_PACKAGES)
        string(MAKE_C_IDENTIFIER "${_capi_app_pdep}" _capi_app_pdep_id)
        string(TOUPPER "${_capi_app_pdep_id}" _capi_app_pdep_id)
        if(DEFINED ${_capi_app_pdep_id}_DIR)
            message(STATUS "  ${_capi_app_pdep} (for capi-${PLATFORM}): "
                           "already resolved by the application")
            continue()
        endif()
        _capi_resolve_named("${_capi_app_pdep}" "${_capi_app_platform_dir}"
                            ${_capi_app_pdep_id}_DIR)
    endforeach()

    # A board, or any directory above it in boards/, may name its own
    # build.cmake.toolchain; the nearest one wins. For a heterogeneous SoC
    # whose cores want different compilers.
    set(_capi_app_walk "${BOARD}")
    while(_capi_app_walk)
        set(_capi_app_walk_dir "${_capi_app_platform_dir}/boards/${_capi_app_walk}")
        if(EXISTS "${_capi_app_walk_dir}/module.yml")
            capi_parse_module("${_capi_app_walk_dir}/module.yml")
            if(CAPI_MODULE_CMAKE_TOOLCHAIN)
                set(_capi_app_toolchain "${_capi_app_walk_dir}/${CAPI_MODULE_CMAKE_TOOLCHAIN}")
                break()
            endif()
        endif()
        get_filename_component(_capi_app_walk "${_capi_app_walk}" DIRECTORY)
    endwhile()
    if(_capi_app_toolchain)
        set(CMAKE_TOOLCHAIN_FILE "${_capi_app_toolchain}")
    endif()

    # The compiler-check scratch projects re-run the toolchain file and see
    # only the variables named here. A toolchain that dispatches on the
    # selection otherwise fails as "no working C compiler".
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PLATFORM BOARD TARGET)
endmacro()

macro(capi_create_application capi_app_name)
    cmake_parse_arguments(_capi_app "" "" "SOURCES" ${ARGN})

    # Unset means no -O flag at all, not even Debug's -Og.
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()

    set(_capi_app_board_dir "${_capi_app_platform_dir}/boards/${BOARD}")

    # A compound BOARD ("<board>/<core>") nests into boards/ as a real path,
    # but the defconfig filename is its last component only: BOARD=p3_r7/css
    # would otherwise ask for boards/p3_r7/css/p3_r7/css_defconfig.
    get_filename_component(_capi_app_board_leaf "${BOARD}" NAME)

    set(_capi_app_defconfig "${_capi_app_board_dir}/${_capi_app_board_leaf}_defconfig")
    if(NOT EXISTS "${_capi_app_defconfig}")
        message(FATAL_ERROR
            "capi: board defconfig not found at ${_capi_app_defconfig} "
            "(BOARD='${BOARD}'). A board's defconfig is required -- without "
            "it the build silently falls back to Kconfig defaults and fails "
            "much later at link, naming a missing ops symbol rather than the "
            "real cause.")
    endif()

    # Kconfig: the platform's symbols, then this board's defconfig, then the
    # application's project.conf, then CAPI_EXTRA_CONF_FILE in order. A
    # fragment named there was asked for explicitly, so a missing one is an
    # error, unlike an omitted project.conf. Relative paths are taken against
    # the application directory, not CMake's working directory.
    set(_capi_app_extra_confs "")
    foreach(_capi_app_extra_conf IN LISTS CAPI_EXTRA_CONF_FILE)
        get_filename_component(_capi_app_extra_conf_abs
            "${_capi_app_extra_conf}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        if(NOT EXISTS "${_capi_app_extra_conf_abs}")
            message(FATAL_ERROR
                "capi: CAPI_EXTRA_CONF_FILE names ${_capi_app_extra_conf} "
                "(${_capi_app_extra_conf_abs}), which does not exist.")
        endif()
        list(APPEND _capi_app_extra_confs "${_capi_app_extra_conf_abs}")
    endforeach()
    set(CAPI_KCONFIG_SOURCES "${_capi_app_platform_dir}/Kconfig")
    capi_generate_config(
        "${_capi_app_defconfig}"
        "${CMAKE_SOURCE_DIR}/project.conf"
        ${_capi_app_extra_confs})

    # For the platform's own scripts, which cannot use
    # CMAKE_CURRENT_LIST_DIR: inside a function that is the *caller's*
    # directory, and they are called from the application's scope.
    set(CAPI_PLATFORM_DIR "${_capi_app_platform_dir}")
    set(CAPI_BOARD_DIR "${_capi_app_board_dir}")

    # Sets CAPI_<PLATFORM>_BOARD_BSP_SRC/_BSP_DIR, gated by the CONFIG_*
    # variables above, so it comes after them.
    include("${_capi_app_board_dir}/board.cmake")

    add_subdirectory("${CAPI_DIR}" "${CMAKE_BINARY_DIR}/capi")

    add_executable(${capi_app_name}
        ${_capi_app_SOURCES}
        ${CAPI_${_capi_app_platform_upper}_BOARD_BSP_SRC})
    target_include_directories(${capi_app_name} PRIVATE
        ${CAPI_${_capi_app_platform_upper}_BOARD_BSP_DIR})

    # Before the platform package below, whose library may depend on this.
    if(_capi_app_vendor_sdk)
        include("${_capi_app_platform_dir}/${_capi_app_vendor_sdk}")
        cmake_language(CALL "capi_${PLATFORM}_config_sdk" ${capi_app_name})
    endif()

    add_subdirectory("${_capi_app_platform_dir}"
                     "${CMAKE_BINARY_DIR}/capi-${PLATFORM}")
    target_link_libraries(${capi_app_name} PUBLIC capi::core capi::${PLATFORM})

    set(_capi_app_flash_cmake "${_capi_app_platform_dir}/sdk/flash.cmake")
    if(EXISTS "${_capi_app_flash_cmake}")
        include("${_capi_app_flash_cmake}")
        cmake_language(CALL "capi_${PLATFORM}_add_flash_target" ${capi_app_name})
    endif()
endmacro()
