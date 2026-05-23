#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.9)
project(catlib VERSION 1.0.1 DESCRIPTION "library for CTkit")

#----------------------------------------------------------------------------
# Locate sources and headers for this project
#
include_directories(${PROJECT_SOURCE_DIR}/source/include )
file(GLOB sources ${PROJECT_SOURCE_DIR}/source/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/source/include/*.h)

##needs work
##should make static AND shared

add_library(catlib STATIC ${sources} ${headers})
#add_library(catlib SHARED ${sources} ${headers})
target_compile_features(catlib PUBLIC cxx_std_17)

set_target_properties(catlib PROPERTIES
    VERSION ${PROJECT_VERSION}
    #SOVERSION 1
    INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/source/include)

install(TARGETS catlib
  ARCHIVE
  DESTINATION ${PROJECT_SOURCE_DIR}/api/lib)

#install(TARGETS catlib
#  LIBRARY
#  DESTINATION ${PROJECT_SOURCE_DIR}/api/lib)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include
  DESTINATION  ${PROJECT_SOURCE_DIR}/api)

add_subdirectory(./test/DRM ./test/DRM/BUILD)
add_subdirectory(./test/FileIO ./test/FileIO/BUILD)
add_subdirectory(./test/InputParams ./test/InputParams/BUILD)
add_subdirectory(./test/XCATPhantomCompress ./test/XCATPhantomCompress/BUILD)

find_package(Doxygen REQUIRED)

if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/docs)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)

    # Add other Doxygen configurations as needed
    # set(DOXYGEN_INPUT ${CMAKE_SOURCE_DIR}/include)  # Adjust the path to your source files

    # Create a custom target for documentation
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/docs
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()
