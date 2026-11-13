# Embedded MTD INI compatibility layer: no separate SKSE entry point.
add_library(unified_music_mtd STATIC Integration.cpp)
target_compile_features(unified_music_mtd PUBLIC cxx_std_23)
target_include_directories(unified_music_mtd PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(unified_music_mtd PUBLIC CommonLibSSE::CommonLibSSE PRIVATE SimpleIni::SimpleIni)
if(MSVC)
    target_compile_options(unified_music_mtd PRIVATE /permissive- /utf-8 /EHsc)
endif()
