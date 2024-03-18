# gdk library build system

set(LIB_NAME gdk)

set(HEADERS
  ../../include/gdk/gdkcursors.h
  ../../include/gdk/gdk.h
  ../../include/gdk/gdki18n.h
  ../../include/gdk/gdkkeysyms.h
  ../../include/gdk/gdkprivate.h
  ../../include/gdk/gdkrgb.h
  ../../include/gdk/gdktypes.h
  ../../include/gdk/gdkx.h
)

set(SOURCES
  gdk.c
  gdkcc.c
  gdkcolor.c
  gdkcursor.c
  gdkdnd.c
  gdkdraw.c
  gdkevents.c
  gdkfont.c
  gdkgc.c
  gdkglobals.c
  gdkimage.c
  gdkim.c
  gdkinput.c
  gdkpixmap.c
  gdkproperty.c
  gdkrectangle.c
  gdkregion.c
  gdkrgb.c
  gdkselection.c
  gdkvisual.c
  gdkwindow.c
  gdkxid.c
  gxid.c
  gxid_lib.c
)

# libdir name to allow overriding to lib32 / lib64
set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "Specify the name of the directory where libraries are meant to be installed")

add_library(${LIB_NAME} SHARED ${SOURCES})

set(LIBS ${CMAKE_THREAD_LIBS_INIT} X11::Xi)

target_link_libraries(${LIB_NAME} ${LIBS})

add_compile_definitions(HAVE_CONFIG_H)
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/gdk
  ${CMAKE_BINARY_DIR} ${glibretro_INCLUDE_DIRS})

# Enable all warnings/errors
target_compile_options(${LIB_NAME} PRIVATE -Wall -Werror)

set_target_properties(${LIB_NAME} PROPERTIES VERSION ${VERSION} SOVERSION ${LIB_PATCH_LEVEL})
set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME ${LIB_NAME}-${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION})

install(TARGETS ${LIB_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
      install(FILES ${HEADERS} DESTINATION include/${PROJECT_NAME}-${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}/${LIB_NAME})

