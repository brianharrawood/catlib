#----------------------------------------------------------------------------
# Setup the project

cmake_minimum_required(VERSION 3.16)

project(FIOtest LANGUAGES CXX)

#----------------------------------------------------------------------------
# Locate sources and headers for this project
#
set (catlib_includes " ${PROJECT_SOURCE_DIR}/../../source/include")

include_directories(${PROJECT_SOURCE_DIR} ${catlib_includes})

file(GLOB sources ${PROJECT_SOURCE_DIR}/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/*.h)

#----------------------------------------------------------------------------
# Add the executable, and link 
#
set (catlib_library "${PROJECT_SOURCE_DIR}/../../BUILD/libcatlib.a")

add_executable(FIOtest ${sources} ${headers})
target_link_libraries(FIOtest ${catlib_library} crypto pthread z)

# for gprof
#set(CMAKE_CXX_FLAGS "-pg")
# optimize
#set(CMAKE_CXX_FLAGS "-O3")

#----------------------------------------------------------------------------
# Compile Info
#
target_compile_features(FIOtest PUBLIC cxx_std_17)

#----------------------------------------------------------------------------
# External libraries
#link_directories()

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
install(TARGETS FIOtest DESTINATION  ${PROJECT_SOURCE_DIR}/bin)

