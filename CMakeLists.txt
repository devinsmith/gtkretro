# gtk+ 1.2x library build system

# The minimum version is the version supported by the oldest Debian release
# Ubuntu 20.04LTS uses 3.16.3
cmake_minimum_required(VERSION 3.13.4)

#set(CMAKE_VERBOSE_MAKEFILE TRUE)
set(LIB_MAJOR_VERSION 1)
set(LIB_MINOR_VERSION 2)
set(LIB_PATCH_LEVEL 10)
set(VERSION ${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_PATCH_LEVEL})

project(gtk+ VERSION ${VERSION})

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)

# Falling back on pkg-config to get glibretro
pkg_check_modules(glibretro glib)

#include(CheckTypeSize)
include(CheckIncludeFile)

#check_type_size("char" SIZEOF_CHAR)
#check_type_size("int" SIZEOF_INT)
#check_type_size("long" SIZEOF_LONG)
#check_type_size("long long" SIZEOF_LONG_LONG)
#check_type_size("short" SIZEOF_SHORT)
#check_type_size("void *" SIZEOF_VOID_P)

check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/ipc.h HAVE_IPC_H)
check_include_file(sys/shm.h HAVE_SHM_H)
if (X11_XShm_FOUND)
  set(HAVE_XSHM_H 1)
endif()
if (X11_Xshape_FOUND)
  set(HAVE_SHAPE_EXT 1)
endif()

configure_file(config.h.in config.h)
configure_file(gdk.pc.in gdk.pc @ONLY)
configure_file(gtk+.pc.in gtk+.pc @ONLY)

#add_subdirectory(deps)
add_subdirectory(src)

#include(CMakePackageConfigHelpers)

#set(INCLUDE_INSTALL_DIR "include") # relative

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gdk.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gtk+.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

#configure_package_config_file(mlk.cmake.in
#  ${CMAKE_CURRENT_BINARY_DIR}/mlk.cmake
#  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mlk
#  PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR)

