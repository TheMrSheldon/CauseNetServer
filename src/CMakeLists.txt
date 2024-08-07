include(FetchContent)
include(FindPkgConfig)

add_library(causenet)
target_sources(causenet PRIVATE
    causenet/causenet.cpp
    causenet/rest/controller_v1.cpp
)
target_compile_features(causenet PUBLIC cxx_std_23)
target_include_directories(causenet PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)
set_target_properties(causenet PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(causenetexe)
target_sources(causenetexe PRIVATE
    main.cpp
)

target_compile_features(causenetexe PUBLIC cxx_std_23)
target_link_libraries(causenetexe PUBLIC causenet)

# We want to build everything into a single binary
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
if (WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
else()
    # set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()
if (MSVC)
    target_compile_options(causenetexe PUBLIC /MT)
    target_link_options(causenetexe PUBLIC /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT)
else()
    # target_link_libraries(causenetexe PUBLIC "-static")
endif()

##########################################################################################
# Libraries
##########################################################################################
# Drogon
set(BUILD_YAML_CONFIG ON)
FetchContent_Declare(drogon GIT_REPOSITORY https://github.com/drogonframework/drogon.git GIT_TAG v1.9.3)
FetchContent_MakeAvailable(drogon)
target_link_libraries(causenet PUBLIC drogon)
add_dependencies(causenet drogon_ctl)  # Add dependency to their commandline tool since it will be part of our build process
message (STATUS $<TARGET_FILE:drogon_ctl>)
add_custom_command(TARGET causenet PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo  "drogon_ctl is found at $<TARGET_FILE:drogon_ctl>")

# RapidJSON
option(RAPIDJSON_BUILD_DOC OFF)
option(RAPIDJSON_BUILD_EXAMPLES OFF)
option(RAPIDJSON_BUILD_TESTS OFF)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/Tencent/rapidjson/
    GIT_TAG ab1842a
)
FetchContent_MakeAvailable(json)
target_link_libraries(causenet PRIVATE RapidJSON)
target_include_directories(causenet PUBLIC ${json_SOURCE_DIR}/include)

# Boost iostreams for reading WARC-files from ClueWeb12
set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF)
set(BOOST_ENABLE_CMAKE ON)
find_package(Boost 1.45.0 COMPONENTS iostreams REQUIRED)
target_link_libraries(causenetexe PUBLIC Boost::iostreams)