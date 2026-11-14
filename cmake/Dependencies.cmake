include_guard(GLOBAL)
include(FetchContent)

# Keep the previously verified Steam 1.7 SDK and compression-library revisions.
# Optional local Git checkout cache; content is checked before it is used.
set(UNIFIED_MUSIC_DEPS_DIR "" CACHE PATH "Existing pinned dependency Git checkouts")
function(unified_music_dependency name repository revision)
    string(TOUPPER "${name}" key)
    if(UNIFIED_MUSIC_DEPS_DIR AND EXISTS "${UNIFIED_MUSIC_DEPS_DIR}/${name}/.git")
        execute_process(COMMAND git -C "${UNIFIED_MUSIC_DEPS_DIR}/${name}" rev-parse HEAD
            OUTPUT_VARIABLE actual OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
        if(NOT actual STREQUAL revision)
            message(FATAL_ERROR "${name} revision mismatch: ${actual}; expected ${revision}")
        endif()
        set("FETCHCONTENT_SOURCE_DIR_${key}" "${UNIFIED_MUSIC_DEPS_DIR}/${name}" PARENT_SCOPE)
    endif()
    FetchContent_Declare(${name}
        GIT_REPOSITORY "${repository}"
        GIT_TAG "${revision}"
        GIT_PROGRESS TRUE
        OVERRIDE_FIND_PACKAGE)
endfunction()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL ON CACHE BOOL "" FORCE)
set(SPDLOG_WCHAR_SUPPORT ON CACHE BOOL "" FORCE)
unified_music_dependency(spdlog https://github.com/gabime/spdlog.git 486b55554f11c9cccc913e11a87085b2a91f706f)
FetchContent_MakeAvailable(spdlog)

set(COMMONLIB_INI OFF CACHE BOOL "" FORCE)
set(COMMONLIB_JSON OFF CACHE BOOL "" FORCE)
set(COMMONLIB_TOML OFF CACHE BOOL "" FORCE)
set(COMMONLIB_XBYAK OFF CACHE BOOL "" FORCE)
set(COMMONLIB_RANDOM OFF CACHE BOOL "" FORCE)
unified_music_dependency(commonlib-shared https://github.com/libxse/commonlib-shared.git 29fbdb0e2dc548c9ab22f6964981d75090dc9094)
FetchContent_MakeAvailable(commonlib-shared)

set(SKYRIM_SUPPORT_AE ON CACHE BOOL "" FORCE)
unified_music_dependency(CommonLibSSE https://github.com/SeaSparrowOG/CommonLibSSE.git 2415cc998b611d3f0ac15fcfeadca9c83b1bf7f8)
FetchContent_MakeAvailable(CommonLibSSE)

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_OBJECT_LIBS OFF CACHE BOOL "" FORCE)
unified_music_dependency(jsoncpp https://github.com/open-source-parsers/jsoncpp.git 89e2973c754a9c02a49974d839779b151e95afd6)
FetchContent_MakeAvailable(jsoncpp)

# Use MTD's established INI parser rather than maintaining a second grammar.
unified_music_dependency(simpleini https://github.com/brofield/simpleini.git 09c21bda1dc1b578fa55f4a005d79b0afd481296)
FetchContent_MakeAvailable(simpleini)

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
unified_music_dependency(zlib https://github.com/madler/zlib.git 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf)
FetchContent_MakeAvailable(zlib)
target_include_directories(zlibstatic PUBLIC
    "$<BUILD_INTERFACE:${zlib_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${zlib_BINARY_DIR}>")
set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()
