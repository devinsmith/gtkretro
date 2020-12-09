/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>
#include <stdlib.h>
//#include <gmodule.h>
#include "gtkthemes.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "config.h"
#include "gtkintl.h"

typedef struct _GtkThemeEnginePrivate GtkThemeEnginePrivate;

struct _GtkThemeEnginePrivate {
  GtkThemeEngine engine;

  void *name;

  void (*init) (GtkThemeEngine *);
  void (*exit) (void);

  guint refcount;
};

static GHashTable *engine_hash = NULL;

GtkThemeEngine*
gtk_theme_engine_get (const gchar *name)
{
  return NULL;
}

void
gtk_theme_engine_ref (GtkThemeEngine *engine)
{
}

void
gtk_theme_engine_unref (GtkThemeEngine *engine)
{
}
