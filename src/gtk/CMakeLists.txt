# gtk library build system

set(LIB_NAME gtk)

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
  gtkaccelgroup.c
  gtkaccellabel.c
  gtkadjustment.c
  gtkalignment.c
  gtkarg.c
  gtkargcollector.c
  gtkarrow.c
  gtkaspectframe.c
  gtkbbox.c
  gtkbin.c
  gtkbindings.c
  gtkbox.c
  gtkbutton.c
  gtkcalendar.c
  gtkcheckbutton.c
  gtkcheckmenuitem.c
  gtkclist.c
  gtkcolorsel.c
  gtkcombo.c
  gtkcontainer.c
  gtkctree.c
  gtkcurve.c
  gtkdata.c
  gtkdialog.c
  gtkdnd.c
  gtkdrawingarea.c
  gtkeditable.c
  gtkentry.c
  gtkeventbox.c
  gtkfilesel.c
  gtkfixed.c
  gtkfontsel.c
  gtkframe.c
  gtkgamma.c
  gtkgc.c
  gtkhandlebox.c
  gtkhbbox.c
  gtkhbox.c
  gtkhpaned.c
  gtkhruler.c
  gtkhscale.c
  gtkhscrollbar.c
  gtkhseparator.c
  gtkimage.c
  gtkinputdialog.c
  gtkinvisible.c
  gtkitem.c
  gtkitemfactory.c
  gtklabel.c
  gtklayout.c
  gtklist.c
  gtklistitem.c
  gtkmain.c
  gtkmarshal.c
  gtkmenubar.c
  gtkmenu.c
  gtkmenufactory.c
  gtkmenuitem.c
  gtkmenushell.c
  gtkmisc.c
  gtknotebook.c
  gtkobject.c
  gtkoptionmenu.c
  gtkpacker.c
  gtkpaned.c
  gtkpixmap.c
  gtkplug.c
  gtkpreview.c
  gtkprogressbar.c
  gtkprogress.c
  gtkradiobutton.c
  gtkradiomenuitem.c
  gtkrange.c
  gtkrc.c
  gtkruler.c
  gtkscale.c
  gtkscrollbar.c
  gtkscrolledwindow.c
  gtkselection.c
  gtkseparator.c
  gtksignal.c
  gtksocket.c
  gtkspinbutton.c
  gtkstatusbar.c
  gtkstyle.c
  gtktable.c
  gtktearoffmenuitem.c
  gtktext.c
  gtkthemes.c
  gtktipsquery.c
  gtktogglebutton.c
  gtktoolbar.c
  gtktooltips.c
  gtktree.c
  gtktreeitem.c
  gtktypeutils.c
  gtkvbbox.c
  gtkvbox.c
  gtkviewport.c
  gtkvpaned.c
  gtkvruler.c
  gtkvscale.c
  gtkvscrollbar.c
  gtkvseparator.c
  gtkwidget.c
  gtkwindow.c
)

# libdir name to allow overriding to lib32 / lib64
set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "Specify the name of the directory where libraries are meant to be installed")

add_library(${LIB_NAME} SHARED ${SOURCES})

set(LIBS ${CMAKE_THREAD_LIBS_INIT} gdk)

target_link_libraries(${LIB_NAME} ${LIBS})

add_compile_definitions(HAVE_CONFIG_H GTK_DATA_PREFIX="${CMAKE_INSTALL_PREFIX}"
  GTK_LIBDIR="${CMAKE_INSTALL_PREFIX}" GTK_SYSCONFDIR="${CMAKE_INSTALL_PREFIX}")
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/${LIB_NAME}
  ${CMAKE_SOURCE_DIR}/include/gdk
  ${CMAKE_BINARY_DIR} ${glibretro_INCLUDE_DIRS})

# Enable all warnings/errors
target_compile_options(${LIB_NAME} PRIVATE -Wall -Werror)

set_target_properties(${LIB_NAME} PROPERTIES VERSION ${VERSION} SOVERSION ${LIB_PATCH_LEVEL})
set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME ${LIB_NAME}-${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION})

install(TARGETS ${LIB_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
      install(FILES ${HEADERS} DESTINATION include/${PROJECT_NAME}-${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}/${LIB_NAME})

