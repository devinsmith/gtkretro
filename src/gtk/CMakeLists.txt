# gtk library build system

set(LIB_NAME gtk)

set(HEADERS
  ../../include/gtk/gtkaccelgroup.h
  ../../include/gtk/gtkaccellabel.h
  ../../include/gtk/gtkadjustment.h
  ../../include/gtk/gtkalignment.h
  ../../include/gtk/gtkarg.h
  ../../include/gtk/gtkarrow.h
  ../../include/gtk/gtkaspectframe.h
  ../../include/gtk/gtkbbox.h
  ../../include/gtk/gtkbindings.h
  ../../include/gtk/gtkbin.h
  ../../include/gtk/gtkbox.h
  ../../include/gtk/gtkbutton.h
  ../../include/gtk/gtkcalendar.h
  ../../include/gtk/gtkcheckbutton.h
  ../../include/gtk/gtkcheckmenuitem.h
  ../../include/gtk/gtkclist.h
  ../../include/gtk/gtkcolorsel.h
  ../../include/gtk/gtkcombo.h
  ../../include/gtk/gtkcompat.h
  ../../include/gtk/gtkcontainer.h
  ../../include/gtk/gtkctree.h
  ../../include/gtk/gtkcurve.h
  ../../include/gtk/gtkdata.h
  ../../include/gtk/gtkdebug.h
  ../../include/gtk/gtkdialog.h
  ../../include/gtk/gtkdnd.h
  ../../include/gtk/gtkdrawingarea.h
  ../../include/gtk/gtkeditable.h
  ../../include/gtk/gtkentry.h
  ../../include/gtk/gtkenums.h
  ../../include/gtk/gtkeventbox.h
  ../../include/gtk/gtkfeatures.h
  ../../include/gtk/gtkfilesel.h
  ../../include/gtk/gtkfixed.h
  ../../include/gtk/gtkfontsel.h
  ../../include/gtk/gtkframe.h
  ../../include/gtk/gtkgamma.h
  ../../include/gtk/gtkgc.h
  ../../include/gtk/gtk.h
  ../../include/gtk/gtkhandlebox.h
  ../../include/gtk/gtkhbbox.h
  ../../include/gtk/gtkhbox.h
  ../../include/gtk/gtkhpaned.h
  ../../include/gtk/gtkhruler.h
  ../../include/gtk/gtkhscale.h
  ../../include/gtk/gtkhscrollbar.h
  ../../include/gtk/gtkhseparator.h
  ../../include/gtk/gtkimage.h
  ../../include/gtk/gtkinputdialog.h
  ../../include/gtk/gtkinvisible.h
  ../../include/gtk/gtkitemfactory.h
  ../../include/gtk/gtkitem.h
  ../../include/gtk/gtklabel.h
  ../../include/gtk/gtklayout.h
  ../../include/gtk/gtklist.h
  ../../include/gtk/gtklistitem.h
  ../../include/gtk/gtkmain.h
  ../../include/gtk/gtkmarshal.h
  ../../include/gtk/gtkmenubar.h
  ../../include/gtk/gtkmenufactory.h
  ../../include/gtk/gtkmenu.h
  ../../include/gtk/gtkmenuitem.h
  ../../include/gtk/gtkmenushell.h
  ../../include/gtk/gtkmisc.h
  ../../include/gtk/gtknotebook.h
  ../../include/gtk/gtkobject.h
  ../../include/gtk/gtkoptionmenu.h
  ../../include/gtk/gtkpacker.h
  ../../include/gtk/gtkpaned.h
  ../../include/gtk/gtkpixmap.h
  ../../include/gtk/gtkplug.h
  ../../include/gtk/gtkpreview.h
  ../../include/gtk/gtkprivate.h
  ../../include/gtk/gtkprogressbar.h
  ../../include/gtk/gtkprogress.h
  ../../include/gtk/gtkradiobutton.h
  ../../include/gtk/gtkradiomenuitem.h
  ../../include/gtk/gtkrange.h
  ../../include/gtk/gtkrc.h
  ../../include/gtk/gtkruler.h
  ../../include/gtk/gtkscale.h
  ../../include/gtk/gtkscrollbar.h
  ../../include/gtk/gtkscrolledwindow.h
  ../../include/gtk/gtkselection.h
  ../../include/gtk/gtkseparator.h
  ../../include/gtk/gtksignal.h
  ../../include/gtk/gtksocket.h
  ../../include/gtk/gtkspinbutton.h
  ../../include/gtk/gtkstatusbar.h
  ../../include/gtk/gtkstyle.h
  ../../include/gtk/gtktable.h
  ../../include/gtk/gtktearoffmenuitem.h
  ../../include/gtk/gtktext.h
  ../../include/gtk/gtkthemes.h
  ../../include/gtk/gtktipsquery.h
  ../../include/gtk/gtktogglebutton.h
  ../../include/gtk/gtktoolbar.h
  ../../include/gtk/gtktooltips.h
  ../../include/gtk/gtktree.h
  ../../include/gtk/gtktreeitem.h
  ../../include/gtk/gtktypebuiltins.h
  ../../include/gtk/gtktypeutils.h
  ../../include/gtk/gtkvbbox.h
  ../../include/gtk/gtkvbox.h
  ../../include/gtk/gtkviewport.h
  ../../include/gtk/gtkvpaned.h
  ../../include/gtk/gtkvruler.h
  ../../include/gtk/gtkvscale.h
  ../../include/gtk/gtkvscrollbar.h
  ../../include/gtk/gtkvseparator.h
  ../../include/gtk/gtkwidget.h
  ../../include/gtk/gtkwindow.h
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

