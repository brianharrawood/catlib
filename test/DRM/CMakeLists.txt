#----------------------------------------------------------------------------
# Setup the project

cmake_minimum_required(VERSION 3.16)

project(DRMtest LANGUAGES CXX)

#----------------------------------------------------------------------------
# Locate sources and headers for this project
#
set (catlib_includes " ${PROJECT_SOURCE_DIR}/../../source/include")

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${catlib_includes})

file(GLOB sources ${PROJECT_SOURCE_DIR}/*.cc ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/*.h ${PROJECT_SOURCE_DIR}/include/*.h)

#----------------------------------------------------------------------------
# Add the executable, and link 
#
set (catlib_library "${PROJECT_SOURCE_DIR}/../../BUILD/libcatlib.a")

add_executable(DRMtest ${sources} ${headers})

# old ubuntu (22.04) needs libbsd
# if osrelease < Ubuntu 24.04.4
# target_link_libraries(DRMtest ${catlib_library} bsd crypto pthread z)
#else
target_link_libraries(DRMtest ${catlib_library} crypto pthread z)

# for gprof
#set(CMAKE_CXX_FLAGS "-pg")
# optimize
#set(CMAKE_CXX_FLAGS "-O3")

#----------------------------------------------------------------------------
# Compile Info
#
target_compile_features(DRMtest PUBLIC cxx_std_17)

#----------------------------------------------------------------------------
# External libraries
#link_directories()


#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
install(TARGETS DRMtest DESTINATION  ${PROJECT_SOURCE_DIR}/bin)

