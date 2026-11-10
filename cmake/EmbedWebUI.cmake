# EmbedWebUI.cmake — reusable helper that embeds a built web front-end (Vite/React "dist")
# into a JUCE target so the WebBrowserComponent never depends on a dev server or absolute paths.
#
#   embed_web_ui(<target>
#       WEB_ROOT   <dir containing package.json>     (default: CMAKE_CURRENT_SOURCE_DIR)
#       DIST_DIR   <built dist dir>                  (default: WEB_ROOT/dist)
#       NAMESPACE  <C++ namespace for the blob>      (default: WebUIBinaryData)
#       EXCLUDE    <regex> ...                       (paths relative to DIST_DIR to skip;
#                                                     source maps and .DS_Store always skipped))
#
# Behaviour:
#   * If DIST_DIR/index.html is missing (or WEBUI_FORCE_BUILD=ON) and npm is available,
#     runs `npm ci` (or `npm install`) + `npm run build` at configure time.
#   * Packs DIST_DIR into a single zip (paths preserved, rebuilt when any dist file changes)
#     and embeds it with juce_add_binary_data as <NAMESPACE>::webui_zip / webui_zipSize.
#   * Defines WEBUI_EMBEDDED=1 on the target. Serve it with the WebUIResourceProvider helper.

option(WEBUI_FORCE_BUILD "Always rebuild the web UI with npm at configure time" OFF)

function(embed_web_ui target)
    cmake_parse_arguments(ARG "" "WEB_ROOT;DIST_DIR;NAMESPACE" "EXCLUDE" ${ARGN})
    if(NOT ARG_WEB_ROOT)
        set(ARG_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT ARG_DIST_DIR)
        set(ARG_DIST_DIR "${ARG_WEB_ROOT}/dist")
    endif()
    if(NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE WebUIBinaryData)
    endif()

    if(WEBUI_FORCE_BUILD OR NOT EXISTS "${ARG_DIST_DIR}/index.html")
        find_program(NPM_EXECUTABLE NAMES npm npm.cmd)
        if(NOT NPM_EXECUTABLE)
            message(FATAL_ERROR "embed_web_ui: ${ARG_DIST_DIR}/index.html not found and npm is not "
                                "available. Run `npm ci && npm run build` in ${ARG_WEB_ROOT} first.")
        endif()
        if(EXISTS "${ARG_WEB_ROOT}/package-lock.json")
            set(_install ci)
        else()
            set(_install install)
        endif()
        message(STATUS "embed_web_ui: building web UI (npm ${_install} && npm run build)")
        execute_process(COMMAND "${NPM_EXECUTABLE}" ${_install} --no-audit --no-fund
                        WORKING_DIRECTORY "${ARG_WEB_ROOT}" RESULT_VARIABLE _r1)
        execute_process(COMMAND "${NPM_EXECUTABLE}" run build
                        WORKING_DIRECTORY "${ARG_WEB_ROOT}" RESULT_VARIABLE _r2)
        if(NOT _r1 EQUAL 0 OR NOT _r2 EQUAL 0 OR NOT EXISTS "${ARG_DIST_DIR}/index.html")
            message(FATAL_ERROR "embed_web_ui: web UI build failed (npm results: ${_r1}, ${_r2})")
        endif()
    endif()

    file(GLOB_RECURSE _abs_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${ARG_DIST_DIR}/*")
    set(_rel_files "")
    foreach(_f IN LISTS _abs_files)
        file(RELATIVE_PATH _rel "${ARG_DIST_DIR}" "${_f}")
        set(_skip FALSE)
        foreach(_re IN LISTS ARG_EXCLUDE ITEMS "\\.map$" "(^|/)\\.DS_Store$")
            if(_rel MATCHES "${_re}")
                set(_skip TRUE)
            endif()
        endforeach()
        if(_skip)
            list(REMOVE_ITEM _abs_files "${_f}")
        else()
            list(APPEND _rel_files "${_rel}")
        endif()
    endforeach()
    list(LENGTH _rel_files _count)
    message(STATUS "embed_web_ui: embedding ${_count} files from ${ARG_DIST_DIR} into ${target}")

    set(_zip_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_webui")
    set(_zip "${_zip_dir}/webui.zip")
    file(MAKE_DIRECTORY "${_zip_dir}")
    add_custom_command(
        OUTPUT "${_zip}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${_zip}"
        COMMAND "${CMAKE_COMMAND}" -E tar cf "${_zip}" --format=zip -- ${_rel_files}
        WORKING_DIRECTORY "${ARG_DIST_DIR}"
        DEPENDS ${_abs_files}
        COMMENT "Packing web UI (${_count} files) for ${target}"
        VERBATIM)

    juce_add_binary_data(${target}_WebUI
        HEADER_NAME WebUIBinaryData.h
        NAMESPACE ${ARG_NAMESPACE}
        SOURCES "${_zip}")
    set_target_properties(${target}_WebUI PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_link_libraries(${target} PRIVATE ${target}_WebUI)
    target_compile_definitions(${target} PRIVATE WEBUI_EMBEDDED=1
                               WEBUI_BINARY_NAMESPACE=${ARG_NAMESPACE})
endfunction()
