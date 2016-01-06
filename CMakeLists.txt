cmake_minimum_required(VERSION 2.8)

project(mapBG)

include_directories(src)
include_directories(include)

# they are rebuilt completely from scratch inside our build-tree
# each subdir is a "project", so some standard variables are defined therein (like protocol_SOURCE_DIR for example)
# but we ourself may be add_subdirectory'ed, with some of the targets already present. so check this!

# This is not possible without mars cmake file :/ So we have to compile this on our own
if(NOT TARGET c_bagel)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ext/c_bagel c_bagel)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ext/c_bagel/src)
endif(NOT TARGET c_bagel)
if(NOT TARGET hwGraph)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../hwGraph hwGraph)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../hwGraph/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../hwGraph/src)
endif(NOT TARGET hwGraph)

SET(ENV{PKG_CONFIG_PATH}
    ${CMAKE_BINARY_DIR}:$ENV{PKG_CONFIG_PATH}
    )

find_package(PkgConfig)
pkg_check_modules(mapBG_PKGCONFIG REQUIRED
    c_bagel hwGraph
    )

include_directories(${mapBG_PKGCONFIG_INCLUDE_DIRS})
link_directories(${mapBG_PKGCONFIG_LIBRARY_DIRS})
add_definitions(${mapBG_PKGCONFIG_CFLAGS_OTHER} -ggdb -Wall -Werror -pedantic)

set(SOURCES
    src/sw_to_hw_map.c
    )
set(HEADERS
    include/sw_to_hw_map.h
    )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(sw2hwmap STATIC ${SOURCES})
target_link_libraries(sw2hwmap
    ${mapBG_PKGCONFIG_LIBRARIES}
    )
# at first we have a minimal cmdline app, which exports simply everything into plaintext files
add_executable(mapper ${SOURCES}
    src/mapper.c
    )
target_link_libraries(mapper
    ${mapBG_PKGCONFIG_LIBRARIES}
    )
add_dependencies(mapper
    c_bagel
    hwGraph
    )

# creating a convenience target, so a user can easily execute ileggui, without cd'ing into the builddir
add_custom_target(mapper-lazy-symlink ALL
    ln --force -s ${CMAKE_CURRENT_BINARY_DIR}/mapper ${CMAKE_SOURCE_DIR}/mapper
    DEPENDS mapper)
# and clean this up if someone asks us to do so
#set_directory_properties(PROPERTIES
    #ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_SOURCE_DIR}/bg2dict_c")
