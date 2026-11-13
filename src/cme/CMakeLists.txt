add_library(unified_music_cme STATIC
    Module.cpp hooks/hooks.cpp settings/JSONSettings.cpp)
target_compile_features(unified_music_cme PUBLIC cxx_std_23)
target_include_directories(unified_music_cme PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(unified_music_cme PUBLIC
    CommonLibSSE::CommonLibSSE JsonCpp::JsonCpp)
if(MSVC)
    target_compile_options(unified_music_cme PRIVATE /EHsc /utf-8 /Zc:preprocessor)
endif()
