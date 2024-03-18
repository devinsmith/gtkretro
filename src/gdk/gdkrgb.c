/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* For more information on GdkRgb, see http://www.levien.com/gdkrgb/

   Raph Levien <raph@acm.org>
   */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENABLE_GRAYSCALE

/* Compiling as a part of Gtk 1.1 or later */
#include "config.h"
#include "gdk.h"
#include "gdkprivate.h"

#include "gdkrgb.h"

typedef struct _GdkRgbInfo   GdkRgbInfo;

typedef void (*GdkRgbConvFunc) (GdkImage *image,
				gint x0, gint y0,
				gint width, gint height,
				guchar *buf, int rowstride,
				gint x_align, gint y_align,
				GdkRgbCmap *cmap);

/* Some of these fields should go, as they're not being used at all.
   Globals should generally migrate into here - it's very likely that
   we'll want to run more than one GdkRgbInfo context at the same time
   (i.e. some but not all windows have privately installed
   colormaps). */

struct _GdkRgbInfo
{
  GdkVisual *visual;
  GdkColormap *cmap;

  gulong *color_pixels;
  gulong *gray_pixels;
  gulong *reserved_pixels;

  guint nred_shades;
  guint ngreen_shades;
  guint nblue_shades;
  guint ngray_shades;
  guint nreserved;

  guint bpp;
  gint cmap_alloced;
  gdouble gamma;

  /* Generally, the stage buffer is used to convert 32bit RGB, gray,
     and indexed images into 24 bit packed RGB. */
  guchar *stage_buf;

  GdkRgbCmap *gray_cmap;

  gboolean dith_default;

  gboolean bitmap; /* set true if in 1 bit per pixel mode */
  GdkGC *own_gc;

  /* Convert functions */
  GdkRgbConvFunc conv;
  GdkRgbConvFunc conv_d;

  GdkRgbConvFunc conv_32;
  GdkRgbConvFunc conv_32_d;

  GdkRgbConvFunc conv_gray;
  GdkRgbConvFunc conv_gray_d;

  GdkRgbConvFunc conv_indexed;
  GdkRgbConvFunc conv_indexed_d;
};

static gboolean gdk_rgb_install_cmap = FALSE;
static gint gdk_rgb_min_colors = 5 * 5 * 5;
static gboolean gdk_rgb_verbose = FALSE;

#define REGION_WIDTH 256
#define STAGE_ROWSTRIDE (REGION_WIDTH * 3)
#define REGION_HEIGHT 64

/* We have N_REGION REGION_WIDTH x REGION_HEIGHT regions divided
 * up between n_images different images. possible_n_images gives
 * various divisors of N_REGIONS. The reason for allowing this
 * flexibility is that we want to create as few images as possible,
 * but we want to deal with the abberant systems that have a SHMMAX
 * limit less than
 *
 * REGION_WIDTH * REGION_HEIGHT * N_REGIONS * 4 (384k)
 *
 * (Are there any such?)
 */
#define N_REGIONS 6
static const int possible_n_images[] = { 1, 2, 3, 6 };

static GdkRgbInfo *image_info = NULL;
static GdkImage *static_image[N_REGIONS];
static gint static_image_idx;
static gint static_n_images;
  

static guchar *colorcube;
static guchar *colorcube_d;

static gint
gdk_rgb_cmap_fail (const char *msg, GdkColormap *cmap, gulong *pixels)
{
  gulong free_pixels[256];
  gint n_free;
  gint i;

#ifdef VERBOSE
  g_print ("%s", msg);
#endif
  n_free = 0;
  for (i = 0; i < 256; i++)
    if (pixels[i] < 256)
      free_pixels[n_free++] = pixels[i];
  if (n_free)
    gdk_colors_free (cmap, free_pixels, n_free, 0);
  return 0;
}

static void
gdk_rgb_make_colorcube (gulong *pixels, gint nr, gint ng, gint nb)
{
  guchar rt[16], gt[16], bt[16];
  gint i;

  colorcube = g_new (guchar, 4096);
  for (i = 0; i < 16; i++)
    {
      rt[i] = ng * nb * ((i * 17 * (nr - 1) + 128) >> 8);
      gt[i] = nb * ((i * 17 * (ng - 1) + 128) >> 8);
      bt[i] = ((i * 17 * (nb - 1) + 128) >> 8);
    }

  for (i = 0; i < 4096; i++)
    {
      colorcube[i] = pixels[rt[i >> 8] + gt[(i >> 4) & 0x0f] + bt[i & 0x0f]];
#ifdef VERBOSE
      g_print ("%03x %02x %x %x %x\n", i, colorcube[i], rt[i >> 8], gt[(i >> 4) & 0x0f], bt[i & 0x0f]);
#endif
    }
}

/* this is the colorcube suitable for dithering */
static void
gdk_rgb_make_colorcube_d (gulong *pixels, gint nr, gint ng, gint nb)
{
  gint r, g, b;
  gint i;

  colorcube_d = g_new (guchar, 512);
  for (i = 0; i < 512; i++)
    {
      r = MIN (nr - 1, i >> 6);
      g = MIN (ng - 1, (i >> 3) & 7);
      b = MIN (nb - 1, i & 7);
      colorcube_d[i] = pixels[(r * ng + g) * nb + b];
    }
}

/* Try installing a color cube of the specified size.
   Make the colorcube and return TRUE on success */
static gint
gdk_rgb_try_colormap (gint nr, gint ng, gint nb)
{
  gint r, g, b;
  gint ri, gi, bi;
  gint r0, g0, b0;
  GdkColormap *cmap;
  GdkColor color;
  gulong pixels[256];
  gulong junk[256];
  gint i;
  gint d2;
  gint colors_needed;
  gint idx;
  gint best[256];

  if (!image_info->cmap_alloced && nr * ng * nb < gdk_rgb_min_colors)
    return FALSE;

  if (image_info->cmap_alloced)
    cmap = image_info->cmap;
  else
    cmap = gdk_colormap_get_system ();

  gdk_colormap_sync (cmap, FALSE);
  
  colors_needed = nr * ng * nb;
  for (i = 0; i < 256; i++)
    {
      best[i] = 192;
      pixels[i] = 256;
    }

#ifndef GAMMA
  if (!gdk_rgb_install_cmap)
  /* find color cube colors that are already present */
  for (i = 0; i < MIN (256, cmap->size); i++)
    {
      r = cmap->colors[i].red >> 8;
      g = cmap->colors[i].green >> 8;
      b = cmap->colors[i].blue >> 8;
      ri = (r * (nr - 1) + 128) >> 8;
      gi = (g * (ng - 1) + 128) >> 8;
      bi = (b * (nb - 1) + 128) >> 8;
      r0 = ri * 255 / (nr - 1);
      g0 = gi * 255 / (ng - 1);
      b0 = bi * 255 / (nb - 1);
      idx = ((ri * nr) + gi) * nb + bi;
      d2 = (r - r0) * (r - r0) + (g - g0) * (g - g0) + (b - b0) * (b - b0);
      if (d2 < best[idx]) {
	if (pixels[idx] < 256)
	  gdk_colors_free (cmap, pixels + idx, 1, 0);
	else
	  colors_needed--;
	color = cmap->colors[i];
	if (!gdk_color_alloc (cmap, &color))
	  return gdk_rgb_cmap_fail ("error allocating system color\n",
				    cmap, pixels);
	pixels[idx] = color.pixel; /* which is almost certainly i */
	best[idx] = d2;
      }
    }
#endif

  if (colors_needed)
    {
      if (!gdk_colors_alloc (cmap, 0, NULL, 0, junk, colors_needed))
	{
	  char tmp_str[80];
	  
	  sprintf (tmp_str,
		   "%d %d %d colormap failed (in gdk_colors_alloc)\n",
		   nr, ng, nb);
	  return gdk_rgb_cmap_fail (tmp_str, cmap, pixels);
	}

      gdk_colors_free (cmap, junk, colors_needed, 0);
    }

  for (r = 0, i = 0; r < nr; r++)
    for (g = 0; g < ng; g++)
      for (b = 0; b < nb; b++, i++)
	{
	  if (pixels[i] == 256)
	    {
	      color.red = r * 65535 / (nr - 1);
	      color.green = g * 65535 / (ng - 1);
	      color.blue = b * 65535 / (nb - 1);

#ifdef GAMMA
	      color.red = 65535 * pow (color.red / 65535.0, 0.5);
	      color.green = 65535 * pow (color.green / 65535.0, 0.5);
	      color.blue = 65535 * pow (color.blue / 65535.0, 0.5);
#endif

	      /* This should be a raw XAllocColor call */
	      if (!gdk_color_alloc (cmap, &color))
		{
		  char tmp_str[80];

		  sprintf (tmp_str, "%d %d %d colormap failed\n",
			   nr, ng, nb);
		  return gdk_rgb_cmap_fail (tmp_str,
					    cmap, pixels);
		}
	      pixels[i] = color.pixel;
	    }
#ifdef VERBOSE
	  g_print ("%d: %lx\n", i, pixels[i]);
#endif
	}

  image_info->nred_shades = nr;
  image_info->ngreen_shades = ng;
  image_info->nblue_shades = nb;
  gdk_rgb_make_colorcube (pixels, nr, ng, nb);
  gdk_rgb_make_colorcube_d (pixels, nr, ng, nb);
  return TRUE;
}

/* Return TRUE on success. */
static gboolean
gdk_rgb_do_colormaps (void)
{
  static const gint sizes[][3] = {
    /*    { 6, 7, 6 }, */
    { 6, 6, 6 }, 
    { 6, 6, 5 }, 
    { 6, 6, 4 }, 
    { 5, 5, 5 }, 
    { 5, 5, 4 }, 
    { 4, 4, 4 }, 
    { 4, 4, 3 }, 
    { 3, 3, 3 }, 
    { 2, 2, 2 }
  };
  static const gint n_sizes = sizeof(sizes) / (3 * sizeof(gint));
  gint i;

  for (i = 0; i < n_sizes; i++)
    if (gdk_rgb_try_colormap (sizes[i][0], sizes[i][1], sizes[i][2]))
      return TRUE;
  return FALSE;
}

/* Make a 2 x 2 x 2 colorcube */
static void
gdk_rgb_colorcube_222 (void)
{
  int i;
  GdkColor color;
  GdkColormap *cmap;

  if (image_info->cmap_alloced)
    cmap = image_info->cmap;
  else
    cmap = gdk_colormap_get_system ();

  colorcube_d = g_new (guchar, 512);

  for (i = 0; i < 8; i++)
    {
      color.red = ((i & 4) >> 2) * 65535;
      color.green = ((i & 2) >> 1) * 65535;
      color.blue = (i & 1) * 65535;
      gdk_color_alloc (cmap, &color);
      colorcube_d[((i & 4) << 4) | ((i & 2) << 2) | (i & 1)] = color.pixel;
    }
}

void
gdk_rgb_set_verbose (gboolean verbose)
{
  gdk_rgb_verbose = verbose;
}

void
gdk_rgb_set_install (gboolean install)
{
  gdk_rgb_install_cmap = install;
}

void
gdk_rgb_set_min_colors (gint min_colors)
{
  gdk_rgb_min_colors = min_colors;
}

static const gchar* visual_names[] =
{
  "static gray",
  "grayscale",
  "static color",
  "pseudo color",
  "true color",
  "direct color",
};

/* Return a "score" based on the following criteria (in hex):

   x000 is the quality - 1 is 1bpp, 2 is 4bpp,
                         4 is 8bpp,
			 7 is 15bpp truecolor, 8 is 16bpp truecolor,
			 9 is 24bpp truecolor.
   0x00 is the speed - 1 is the normal case,
                       2 means faster than normal
   00x0 gets a point for being the system visual
   000x gets a point for being pseudocolor

   A caveat: in the 8bpp modes, being the system visual seems to be
   quite important. Thus, all of the 8bpp modes should be ranked at
   the same speed.
*/
static guint32
gdk_rgb_score_visual (GdkVisual *visual)
{
  guint32 quality, speed, sys, pseudo;

  quality = 0;
  speed = 1;
  sys = 0;
  if (visual->type == GDK_VISUAL_TRUE_COLOR ||
      visual->type == GDK_VISUAL_DIRECT_COLOR)
    {
      if (visual->depth == 24)
	{
	  quality = 9;
	  /* Should test for MSB visual here, and set speed if so. */
	}
      else if (visual->depth == 16)
	quality = 8;
      else if (visual->depth == 15)
	quality = 7;
      else if (visual->depth == 8)
	quality = 4;
    }
  else if (visual->type == GDK_VISUAL_PSEUDO_COLOR ||
	   visual->type == GDK_VISUAL_STATIC_COLOR)
    {
      if (visual->depth == 8)
	quality = 4;
      else if (visual->depth == 4)
	quality = 2;
      else if (visual->depth == 1)
	quality = 1;
    }
  else if (visual->type == GDK_VISUAL_STATIC_GRAY
#ifdef ENABLE_GRAYSCALE
	   || visual->type == GDK_VISUAL_GRAYSCALE
#endif
	   )
    {
      if (visual->depth == 8)
	quality = 4;
      else if (visual->depth == 4)
	quality = 2;
      else if (visual->depth == 1)
	quality = 1;
    }

  if (quality == 0)
    return 0;

  sys = (visual == gdk_visual_get_system ());

  pseudo = (visual->type == GDK_VISUAL_PSEUDO_COLOR || visual->type == GDK_VISUAL_TRUE_COLOR);

  if (gdk_rgb_verbose)
    g_print ("Visual 0x%x, type = %s, depth = %d, %x:%x:%x%s; score=%x\n",
	     (gint)(((GdkVisualPrivate *)visual)->xvisual->visualid),
	     visual_names[visual->type],
	     visual->depth,
	     visual->red_mask,
	     visual->green_mask,
	     visual->blue_mask,
	     sys ? " (system)" : "",
	     (quality << 12) | (speed << 8) | (sys << 4) | pseudo);

  return (quality << 12) | (speed << 8) | (sys << 4) | pseudo;
}

static void
gdk_rgb_choose_visual (void)
{
  GList *visuals, *tmp_list;
  guint32 score, best_score;
  GdkVisual *visual, *best_visual;

  visuals = gdk_list_visuals ();
  tmp_list = visuals;

  best_visual = tmp_list->data;
  best_score = gdk_rgb_score_visual (best_visual);
  tmp_list = tmp_list->next;
  while (tmp_list)
    {
      visual = tmp_list->data;
      score = gdk_rgb_score_visual (visual);
      if (score > best_score)
	{
	  best_score = score;
	  best_visual = visual;
	}
      tmp_list = tmp_list->next;
    }

  g_list_free (visuals);

  image_info->visual = best_visual;
}

static void gdk_rgb_select_conv (GdkImage *image);

static void
gdk_rgb_set_gray_cmap (GdkColormap *cmap)
{
  gint i;
  GdkColor color;
#ifdef VERBOSE
  gint status;
#endif
  gulong pixels[256];
  gint r, g, b, gray;

  for (i = 0; i < 256; i++)
    {
      color.pixel = i;
      color.red = i * 257;
      color.green = i * 257;
      color.blue = i * 257;
#ifdef VERBOSE
      status = gdk_color_alloc (cmap, &color);
#endif
      gdk_color_alloc (cmap, &color);
      pixels[i] = color.pixel;
#ifdef VERBOSE
      g_print ("allocating pixel %d, %x %x %x, result %d\n",
	       color.pixel, color.red, color.green, color.blue, status);
#endif
    }

  /* Now, we make fake colorcubes - we ultimately just use the pseudocolor
     methods. */

  colorcube = g_new (guchar, 4096);

  for (i = 0; i < 4096; i++)
    {
      r = (i >> 4) & 0xf0;
      r = r | r >> 4;
      g = i & 0xf0;
      g = g | g >> 4;
      b = (i << 4 & 0xf0);
      b = b | b >> 4;
      gray = (g + ((r + b) >> 1)) >> 1;
      colorcube[i] = pixels[gray];
    }
}

static gboolean
gdk_rgb_allocate_images (gint        n_images,
			 gboolean    shared)
{
  gint i;
  
  for (i = 0; i < n_images; i++)
    {
      if (image_info->bitmap)
	/* Use malloc() instead of g_malloc since X will free() this mem */
	static_image[i] = gdk_image_new_bitmap (image_info->visual,
						(gpointer) malloc (REGION_WIDTH * REGION_HEIGHT >> 3),
						REGION_WIDTH * (N_REGIONS / n_images), REGION_HEIGHT);
      else
	static_image[i] = gdk_image_new (shared ? GDK_IMAGE_SHARED : GDK_IMAGE_NORMAL,
					 image_info->visual,
					 REGION_WIDTH * (N_REGIONS / n_images), REGION_HEIGHT);
      
      if (!static_image[i])
	{
	  gint j;
	  
	  for (j = 0; j < i; j++)
	    gdk_image_destroy (static_image[j]);
	  
	  return FALSE;
	}
    }
  
  return TRUE;
}

void
gdk_rgb_init(void)
{
  gint i;
  static const gint byte_order[1] = { 1 };

  /* check endian sanity */
#if G_BYTE_ORDER == G_BIG_ENDIAN
  if (((char *)byte_order)[0] == 1)
    g_error ("gdk_rgb_init: compiled for big endian, but this is a little endian machine.\n\n");
#else
  if (((char *)byte_order)[0] != 1)
    g_error ("gdk_rgb_init: compiled for little endian, but this is a big endian machine.\n\n");
#endif

  if (image_info == NULL) {
    image_info = g_new0 (GdkRgbInfo, 1);

    image_info->visual = NULL;
    image_info->cmap = NULL;

    image_info->color_pixels = NULL;
    image_info->gray_pixels = NULL;
    image_info->reserved_pixels = NULL;

    image_info->nred_shades = 6;
    image_info->ngreen_shades = 6;
    image_info->nblue_shades = 4;
    image_info->ngray_shades = 24;
    image_info->nreserved = 0;

    image_info->bpp = 0;
    image_info->cmap_alloced = FALSE;
    image_info->gamma = 1.0;

    image_info->stage_buf = NULL;

    image_info->own_gc = NULL;

    gdk_rgb_choose_visual();

    if ((image_info->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
        image_info->visual->type == GDK_VISUAL_STATIC_COLOR) &&
        image_info->visual->depth < 8 && image_info->visual->depth >= 3) {
      image_info->cmap = gdk_colormap_get_system ();
      gdk_rgb_colorcube_222 ();
    } else if (image_info->visual->type == GDK_VISUAL_PSEUDO_COLOR) {
      if (gdk_rgb_install_cmap || image_info->visual != gdk_visual_get_system()) {
        image_info->cmap = gdk_colormap_new (image_info->visual, FALSE);
        image_info->cmap_alloced = TRUE;
      }
      if (!gdk_rgb_do_colormaps ()) {
        image_info->cmap = gdk_colormap_new (image_info->visual, FALSE);
        image_info->cmap_alloced = TRUE;
        gdk_rgb_do_colormaps ();
      }
      if (gdk_rgb_verbose) {
        g_print ("color cube: %d x %d x %d\n",
          image_info->nred_shades,
          image_info->ngreen_shades,
          image_info->nblue_shades);
      }

      if (!image_info->cmap_alloced)
        image_info->cmap = gdk_colormap_get_system ();
    }
#ifdef ENABLE_GRAYSCALE
    else if (image_info->visual->type == GDK_VISUAL_GRAYSCALE) {
      image_info->cmap = gdk_colormap_new (image_info->visual, FALSE);
      gdk_rgb_set_gray_cmap (image_info->cmap);
      image_info->cmap_alloced = TRUE;
    }
#endif
    else
    {
      /* Always install colormap in direct color. */
      if (image_info->visual->type != GDK_VISUAL_DIRECT_COLOR &&
          image_info->visual == gdk_visual_get_system ())
        image_info->cmap = gdk_colormap_get_system ();
      else {
        image_info->cmap = gdk_colormap_new (image_info->visual, FALSE);
        image_info->cmap_alloced = TRUE;
      }
    }

    image_info->bitmap = (image_info->visual->depth == 1);

    /* Try to allocate as few possible shared images */
    for (i=0; i < sizeof (possible_n_images) / sizeof (possible_n_images[0]); i++) {
      if (gdk_rgb_allocate_images (possible_n_images[i], TRUE)) {
        static_n_images = possible_n_images[i];
        break;
      }
    }

    /* If that fails, just allocate N_REGIONS normal images */
    if (i == sizeof (possible_n_images) / sizeof (possible_n_images[0])) {
      gdk_rgb_allocate_images (N_REGIONS, FALSE);
      static_n_images = N_REGIONS;
    }

    image_info->bpp = static_image[0]->bpp;

    gdk_rgb_select_conv(static_image[0]);
  }
}

/* convert an rgb value into an X pixel code */
gulong
gdk_rgb_xpixel_from_rgb (guint32 rgb)
{
  gulong pixel = 0;

  if (image_info->bitmap)
    {
      return ((rgb & 0xff0000) >> 16) +
	((rgb & 0xff00) >> 7) +
	(rgb & 0xff) > 510;
    }
  else if (image_info->visual->type == GDK_VISUAL_PSEUDO_COLOR)
    pixel = colorcube[((rgb & 0xf00000) >> 12) |
		     ((rgb & 0xf000) >> 8) |
		     ((rgb & 0xf0) >> 4)];
  else if (image_info->visual->depth < 8 &&
	   image_info->visual->type == GDK_VISUAL_STATIC_COLOR)
    {
      pixel = colorcube_d[((rgb & 0x800000) >> 17) |
			 ((rgb & 0x8000) >> 12) |
			 ((rgb & 0x80) >> 7)];
    }
  else if (image_info->visual->type == GDK_VISUAL_TRUE_COLOR ||
	   image_info->visual->type == GDK_VISUAL_DIRECT_COLOR)
    {
#ifdef VERBOSE
      g_print ("shift, prec: r %d %d g %d %d b %d %d\n",
	       image_info->visual->red_shift,
	       image_info->visual->red_prec,
	       image_info->visual->green_shift,
	       image_info->visual->green_prec,
	       image_info->visual->blue_shift,
	       image_info->visual->blue_prec);
#endif

      pixel = (((((rgb & 0xff0000) >> 16) >>
		 (8 - image_info->visual->red_prec)) <<
		image_info->visual->red_shift) +
	       ((((rgb & 0xff00) >> 8)  >>
		 (8 - image_info->visual->green_prec)) <<
		image_info->visual->green_shift) +
	       (((rgb & 0xff) >>
		 (8 - image_info->visual->blue_prec)) <<
		image_info->visual->blue_shift));
    }
  else if (image_info->visual->type == GDK_VISUAL_STATIC_GRAY ||
	   image_info->visual->type == GDK_VISUAL_GRAYSCALE)
    {
      int gray = ((rgb & 0xff0000) >> 16) +
	((rgb & 0xff00) >> 7) +
	(rgb & 0xff);

      return gray >> (10 - image_info->visual->depth);
    }

  return pixel;
}

void
gdk_rgb_gc_set_foreground (GdkGC *gc, guint32 rgb)
{
  GdkColor color;

  color.pixel = gdk_rgb_xpixel_from_rgb (rgb);
  gdk_gc_set_foreground (gc, &color);
}

void
gdk_rgb_gc_set_background (GdkGC *gc, guint32 rgb)
{
  GdkColor color;

  color.pixel = gdk_rgb_xpixel_from_rgb (rgb);
  gdk_gc_set_background (gc, &color);
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_8
#endif

#ifdef HAIRY_CONVERT_8
static void
gdk_rgb_convert_8 (GdkImage *image,
		   gint x0, gint y0, gint width, gint height,
		   guchar *buf, int rowstride,
		   gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      obptr[0] = colorcube[((r & 0xf0) << 4) |
				  (g & 0xf0) |
				  (b >> 4)];
	      obptr++;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      guint32 r1b0g0r0;
	      guint32 g2r2b1g1;
	      guint32 b3g3r3b2;

	      r1b0g0r0 = ((guint32 *)bp2)[0];
	      g2r2b1g1 = ((guint32 *)bp2)[1];
	      b3g3r3b2 = ((guint32 *)bp2)[2];
	      ((guint32 *)obptr)[0] =
		colorcube[((r1b0g0r0 & 0xf0) << 4) | 
			 ((r1b0g0r0 & 0xf000) >> 8) |
			 ((r1b0g0r0 & 0xf00000) >> 20)] |
		(colorcube[((r1b0g0r0 & 0xf0000000) >> 20) |
			  (g2r2b1g1 & 0xf0) |
			  ((g2r2b1g1 & 0xf000) >> 12)] << 8) |
		(colorcube[((g2r2b1g1 & 0xf00000) >> 12) |
			  ((g2r2b1g1 & 0xf0000000) >> 24) |
			  ((b3g3r3b2 & 0xf0) >> 4)] << 16) |
		(colorcube[((b3g3r3b2 & 0xf000) >> 4) |
			  ((b3g3r3b2 & 0xf00000) >> 16) |
			  (b3g3r3b2 >> 28)] << 24);
	      bp2 += 12;
	      obptr += 4;
	    }
	  for (; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      obptr[0] = colorcube[((r & 0xf0) << 4) |
				  (g & 0xf0) |
				  (b >> 4)];
	      obptr++;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
gdk_rgb_convert_8 (GdkImage *image,
		   gint x0, gint y0, gint width, gint height,
		   guchar *buf, int rowstride,
		   gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  obptr[0] = colorcube[((r & 0xf0) << 4) |
			      (g & 0xf0) |
			      (b >> 4)];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

static void
gdk_rgb_convert_8_d666 (GdkImage *image,
			gint x0, gint y0, gint width, gint height,
			guchar *buf, int rowstride,
			gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  const guchar *dmp;
  gint dith;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) | 7;
	  r = ((r * 5) + dith) >> 8;
	  g = ((g * 5) + (262 - dith)) >> 8;
	  b = ((b * 5) + dith) >> 8;
	  obptr[0] = colorcube_d[(r << 6) | (g << 3) | b];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_8_d (GdkImage *image,
		     gint x0, gint y0, gint width, gint height,
		     guchar *buf, int rowstride,
		     gint x_align, gint y_align,
		     GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  const guchar *dmp;
  gint dith;
  gint rs, gs, bs;

  bptr = buf;
  bpl = image->bpl;
  rs = image_info->nred_shades - 1;
  gs = image_info->ngreen_shades - 1;
  bs = image_info->nblue_shades - 1;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) | 7;
	  r = ((r * rs) + dith) >> 8;
	  g = ((g * gs) + (262 - dith)) >> 8;
	  b = ((b * bs) + dith) >> 8;
	  obptr[0] = colorcube_d[(r << 6) | (g << 3) | b];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_8_indexed (GdkImage *image,
			   gint x0, gint y0, gint width, gint height,
			   guchar *buf, int rowstride,
			   gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  guchar c;
  guchar *lut;

  lut = cmap->lut;
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  c = *bp2++;
	  obptr[0] = lut[c];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_gray8 (GdkImage *image,
		       gint x0, gint y0, gint width, gint height,
		       guchar *buf, int rowstride,
		       gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  obptr[0] = (g + ((b + r) >> 1)) >> 1;
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_gray8_gray (GdkImage *image,
			    gint x0, gint y0, gint width, gint height,
			    guchar *buf, int rowstride,
			    gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int y;
  gint bpl;
  guchar *obuf;
  guchar *bptr;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      memcpy (obuf, bptr, width);
      bptr += rowstride;
      obuf += bpl;
    }
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_565
#endif

#ifdef HAIRY_CONVERT_565
/* Render a 24-bit RGB image in buf into the GdkImage, without dithering.
   This assumes native byte ordering - what should really be done is to
   check whether static_image->byte_order is consistent with the _ENDIAN
   config flag, and if not, use a different function.

   This one is even faster than the one below - its inner loop loads 3
   words (i.e. 4 24-bit pixels), does a lot of shifting and masking,
   then writes 2 words. */
static void
gdk_rgb_convert_565 (GdkImage *image,
		     gint x0, gint y0, gint width, gint height,
		     guchar *buf, int rowstride,
		     gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  guchar r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      ((guint16 *)obptr)[0] = ((r & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(b >> 3);
	      obptr += 2;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      guint32 r1b0g0r0;
	      guint32 g2r2b1g1;
	      guint32 b3g3r3b2;

	      r1b0g0r0 = ((guint32 *)bp2)[0];
	      g2r2b1g1 = ((guint32 *)bp2)[1];
	      b3g3r3b2 = ((guint32 *)bp2)[2];
	      ((guint32 *)obptr)[0] =
		((r1b0g0r0 & 0xf8) << 8) |
		((r1b0g0r0 & 0xfc00) >> 5) |
		((r1b0g0r0 & 0xf80000) >> 19) |
		(r1b0g0r0 & 0xf8000000) |
		((g2r2b1g1 & 0xfc) << 19) |
		((g2r2b1g1 & 0xf800) << 5);
	      ((guint32 *)obptr)[1] =
		((g2r2b1g1 & 0xf80000) >> 8) |
		((g2r2b1g1 & 0xfc000000) >> 21) |
		((b3g3r3b2 & 0xf8) >> 3) |
		((b3g3r3b2 & 0xf800) << 16) |
		((b3g3r3b2 & 0xfc0000) << 3) |
		((b3g3r3b2 & 0xf8000000) >> 11);
	      bp2 += 12;
	      obptr += 8;
	    }
	  for (; x < width; x++)
	    {
	      r = *bp2++;
	      g = *bp2++;
	      b = *bp2++;
	      ((guint16 *)obptr)[0] = ((r & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(b >> 3);
	      obptr += 2;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
/* Render a 24-bit RGB image in buf into the GdkImage, without dithering.
   This assumes native byte ordering - what should really be done is to
   check whether static_image->byte_order is consistent with the _ENDIAN
   config flag, and if not, use a different function.

   This routine is faster than the one included with Gtk 1.0 for a number
   of reasons:

   1. Shifting instead of lookup tables (less memory traffic).

   2. Much less register pressure, especially because shifts are
   in the code.

   3. A memcpy is avoided (i.e. the transfer function).

   4. On big-endian architectures, byte swapping is avoided.

   That said, it wouldn't be hard to make it even faster - just make an
   inner loop that reads 3 words (i.e. 4 24-bit pixels), does a lot of
   shifting and masking, then writes 2 words.
*/
static void
gdk_rgb_convert_565 (GdkImage *image,
		     gint x0, gint y0, gint width, gint height,
		     guchar *buf, int rowstride,
		     gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  guchar r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  ((unsigned short *)obuf)[x] = ((r & 0xf8) << 8) |
	    ((g & 0xfc) << 3) |
	    (b >> 3);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

#ifdef HAIRY_CONVERT_565
static void
gdk_rgb_convert_565_gray (GdkImage *image,
			  gint x0, gint y0, gint width, gint height,
			  guchar *buf, int rowstride,
			  gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  guchar g;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      g = *bp2++;
	      ((guint16 *)obptr)[0] = ((g & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(g >> 3);
	      obptr += 2;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      guint32 g3g2g1g0;

	      g3g2g1g0 = ((guint32 *)bp2)[0];
	      ((guint32 *)obptr)[0] =
		((g3g2g1g0 & 0xf8) << 8) |
		((g3g2g1g0 & 0xfc) << 3) |
		((g3g2g1g0 & 0xf8) >> 3) |
		(g3g2g1g0 & 0xf800) << 16 |
		((g3g2g1g0 & 0xfc00) << 11) |
		((g3g2g1g0 & 0xf800) << 5);
	      ((guint32 *)obptr)[1] =
		((g3g2g1g0 & 0xf80000) >> 8) |
		((g3g2g1g0 & 0xfc0000) >> 13) |
		((g3g2g1g0 & 0xf80000) >> 19) |
		(g3g2g1g0 & 0xf8000000) |
		((g3g2g1g0 & 0xfc000000) >> 5) |
		((g3g2g1g0 & 0xf8000000) >> 11);
	      bp2 += 4;
	      obptr += 8;
	    }
	  for (; x < width; x++)
	    {
	      g = *bp2++;
	      ((guint16 *)obptr)[0] = ((g & 0xf8) << 8) |
		((g & 0xfc) << 3) |
		(g >> 3);
	      obptr += 2;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
gdk_rgb_convert_565_gray (GdkImage *image,
			  gint x0, gint y0, gint width, gint height,
			  guchar *buf, int rowstride,
			  gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  guchar g;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  g = *bp2++;
	  ((guint16 *)obuf)[x] = ((g & 0xf8) << 8) |
	    ((g & 0xfc) << 3) |
	    (g >> 3);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

static void
gdk_rgb_convert_565_br (GdkImage *image,
			gint x0, gint y0, gint width, gint height,
			guchar *buf, int rowstride,
			gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  guchar r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  /* final word is:
	     g4 g3 g2 b7 b6 b5 b4 b3  r7 r6 r5 r4 r3 g7 g6 g5
	   */
	  ((unsigned short *)obuf)[x] = (r & 0xf8) |
	    ((g & 0xe0) >> 5) |
	    ((g & 0x1c) << 11) |
	    ((b & 0xf8) << 5);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* Thanks to Ray Lehtiniemi for a patch that resulted in a ~25% speedup
   in this mode. */
#ifdef HAIRY_CONVERT_565
static void
gdk_rgb_convert_565_d (GdkImage *image,
		     gint x0, gint y0, gint width, gint height,
		     guchar *buf, int rowstride,
		     gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  /* Now this is what I'd call some highly tuned code! */
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;

  width += x_align;
  height += y_align;
  
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = y_align; y < height; y++)
    {
      guint32 *dmp = DM_565 + ((y & (DM_HEIGHT - 1)) << DM_WIDTH_SHIFT);
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = x_align; x < width; x++)
	    {
	      gint32 rgb = *bp2++ << 20;
	      rgb += *bp2++ << 10;
	      rgb += *bp2++;
	      rgb += dmp[x & (DM_WIDTH - 1)];
	      rgb += 0x10040100
		- ((rgb & 0x1e0001e0) >> 5)
		- ((rgb & 0x00070000) >> 6);

	      ((unsigned short *)obptr)[0] =
		((rgb & 0x0f800000) >> 12) |
		((rgb & 0x0003f000) >> 7) |
		((rgb & 0x000000f8) >> 3);
	      obptr += 2;
	    }
	}
      else
	{
	  for (x = x_align; x < width - 3; x += 4)
	    {
	      guint32 r1b0g0r0;
	      guint32 g2r2b1g1;
	      guint32 b3g3r3b2;
	      guint32 rgb02, rgb13;

	      r1b0g0r0 = ((guint32 *)bp2)[0];
	      g2r2b1g1 = ((guint32 *)bp2)[1];
	      b3g3r3b2 = ((guint32 *)bp2)[2];
	      rgb02 =
		((r1b0g0r0 & 0xff) << 20) +
		((r1b0g0r0 & 0xff00) << 2) +
		((r1b0g0r0 & 0xff0000) >> 16) +
		dmp[x & (DM_WIDTH - 1)];
	      rgb02 += 0x10040100
		- ((rgb02 & 0x1e0001e0) >> 5)
		- ((rgb02 & 0x00070000) >> 6);
	      rgb13 =
		((r1b0g0r0 & 0xff000000) >> 4) +
		((g2r2b1g1 & 0xff) << 10) +
		((g2r2b1g1 & 0xff00) >> 8) +
		dmp[(x + 1) & (DM_WIDTH - 1)];
	      rgb13 += 0x10040100
		- ((rgb13 & 0x1e0001e0) >> 5)
		- ((rgb13 & 0x00070000) >> 6);
	      ((guint32 *)obptr)[0] =
		((rgb02 & 0x0f800000) >> 12) |
		((rgb02 & 0x0003f000) >> 7) |
		((rgb02 & 0x000000f8) >> 3) |
		((rgb13 & 0x0f800000) << 4) |
		((rgb13 & 0x0003f000) << 9) |
		((rgb13 & 0x000000f8) << 13);
	      rgb02 =
		((g2r2b1g1 & 0xff0000) << 4) +
		((g2r2b1g1 & 0xff000000) >> 14) +
		(b3g3r3b2 & 0xff) +
		dmp[(x + 2) & (DM_WIDTH - 1)];
	      rgb02 += 0x10040100
		- ((rgb02 & 0x1e0001e0) >> 5)
		- ((rgb02 & 0x00070000) >> 6);
	      rgb13 =
		((b3g3r3b2 & 0xff00) << 12) +
		((b3g3r3b2 & 0xff0000) >> 6) +
		((b3g3r3b2 & 0xff000000) >> 24) +
		dmp[(x + 3) & (DM_WIDTH - 1)];
	      rgb13 += 0x10040100
		- ((rgb13 & 0x1e0001e0) >> 5)
		- ((rgb13 & 0x00070000) >> 6);
	      ((guint32 *)obptr)[1] =
		((rgb02 & 0x0f800000) >> 12) |
		((rgb02 & 0x0003f000) >> 7) |
		((rgb02 & 0x000000f8) >> 3) |
		((rgb13 & 0x0f800000) << 4) |
		((rgb13 & 0x0003f000) << 9) |
		((rgb13 & 0x000000f8) << 13);
	      bp2 += 12;
	      obptr += 8;
	    }
	  for (; x < width; x++)
	    {
	      gint32 rgb = *bp2++ << 20;
	      rgb += *bp2++ << 10;
	      rgb += *bp2++;
	      rgb += dmp[x & (DM_WIDTH - 1)];
	      rgb += 0x10040100
		- ((rgb & 0x1e0001e0) >> 5)
		- ((rgb & 0x00070000) >> 6);

	      ((unsigned short *)obptr)[0] =
		((rgb & 0x0f800000) >> 12) |
		((rgb & 0x0003f000) >> 7) |
		((rgb & 0x000000f8) >> 3);
	      obptr += 2;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
gdk_rgb_convert_565_d (GdkImage *image,
                       gint x0, gint y0, gint width, gint height,
                       guchar *buf, int rowstride,
                       gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr;

  width += x_align;
  height += y_align;
  
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + (x0 - x_align) * 2;

  for (y = y_align; y < height; y++)
    {
      guint32 *dmp = DM_565 + ((y & (DM_HEIGHT - 1)) << DM_WIDTH_SHIFT);
      guchar *bp2 = bptr;

      for (x = x_align; x < width; x++)
        {
          gint32 rgb = *bp2++ << 20;
          rgb += *bp2++ << 10;
          rgb += *bp2++;
	  rgb += dmp[x & (DM_WIDTH - 1)];
          rgb += 0x10040100
            - ((rgb & 0x1e0001e0) >> 5)
            - ((rgb & 0x00070000) >> 6);

          ((unsigned short *)obuf)[x] =
            ((rgb & 0x0f800000) >> 12) |
            ((rgb & 0x0003f000) >> 7) |
            ((rgb & 0x000000f8) >> 3);
        }

      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

static void
gdk_rgb_convert_555 (GdkImage *image,
		     gint x0, gint y0, gint width, gint height,
		     guchar *buf, int rowstride,
		     gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  guchar r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  ((unsigned short *)obuf)[x] = ((r & 0xf8) << 7) |
	    ((g & 0xf8) << 2) |
	    (b >> 3);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_555_br (GdkImage *image,
			gint x0, gint y0, gint width, gint height,
			guchar *buf, int rowstride,
			gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  guchar r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 2;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  /* final word is:
	     g5 g4 g3 b7 b6 b5 b4 b3  0 r7 r6 r5 r4 r3 g7 g6
	   */
	  ((unsigned short *)obuf)[x] = ((r & 0xf8) >> 1) |
	    ((g & 0xc0) >> 6) |
	    ((g & 0x38) << 10) |
	    ((b & 0xf8) << 5);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_888_msb (GdkImage *image,
			 gint x0, gint y0, gint width, gint height,
			 guchar *buf, int rowstride,
			 gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int y;
  guchar *obuf;
  gint bpl;
  guchar *bptr;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 3;
  for (y = 0; y < height; y++)
    {
      memcpy (obuf, bptr, width + width + width);
      bptr += rowstride;
      obuf += bpl;
    }
}

/* todo: optimize this */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define HAIRY_CONVERT_888
#endif

#ifdef HAIRY_CONVERT_888
static void
gdk_rgb_convert_888_lsb (GdkImage *image,
			 gint x0, gint y0, gint width, gint height,
			 guchar *buf, int rowstride,
			 gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 3;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      if (((unsigned long)obuf | (unsigned long) bp2) & 3)
	{
	  for (x = 0; x < width; x++)
	    {
	      r = bp2[0];
	      g = bp2[1];
	      b = bp2[2];
	      *obptr++ = b;
	      *obptr++ = g;
	      *obptr++ = r;
	      bp2 += 3;
	    }
	}
      else
	{
	  for (x = 0; x < width - 3; x += 4)
	    {
	      guint32 r1b0g0r0;
	      guint32 g2r2b1g1;
	      guint32 b3g3r3b2;

	      r1b0g0r0 = ((guint32 *)bp2)[0];
	      g2r2b1g1 = ((guint32 *)bp2)[1];
	      b3g3r3b2 = ((guint32 *)bp2)[2];
	      ((guint32 *)obptr)[0] =
		(r1b0g0r0 & 0xff00) |
		((r1b0g0r0 & 0xff0000) >> 16) |
		(((g2r2b1g1 & 0xff00) | (r1b0g0r0 & 0xff)) << 16);
	      ((guint32 *)obptr)[1] =
		(g2r2b1g1 & 0xff0000ff) |
		((r1b0g0r0 & 0xff000000) >> 16) |
		((b3g3r3b2 & 0xff) << 16);
	      ((guint32 *)obptr)[2] =
		(((g2r2b1g1 & 0xff0000) | (b3g3r3b2 & 0xff000000)) >> 16) |
		((b3g3r3b2 & 0xff00) << 16) |
		((b3g3r3b2 & 0xff0000));
	      bp2 += 12;
	      obptr += 12;
	    }
	  for (; x < width; x++)
	    {
	      r = bp2[0];
	      g = bp2[1];
	      b = bp2[2];
	      *obptr++ = b;
	      *obptr++ = g;
	      *obptr++ = r;
	      bp2 += 3;
	    }
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#else
static void
gdk_rgb_convert_888_lsb (GdkImage *image,
			 gint x0, gint y0, gint width, gint height,
			 guchar *buf, int rowstride,
			 gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 3;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  obuf[x * 3] = b;
	  obuf[x * 3 + 1] = g;
	  obuf[x * 3 + 2] = r;
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}
#endif

/* convert 24-bit packed to 32-bit unpacked */
/* todo: optimize this */
static void
gdk_rgb_convert_0888 (GdkImage *image,
		      gint x0, gint y0, gint width, gint height,
		      guchar *buf, int rowstride,
		      gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 4;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  ((guint32 *)obuf)[x] = (r << 16) | (g << 8) | b;
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_0888_br (GdkImage *image,
			 gint x0, gint y0, gint width, gint height,
			 guchar *buf, int rowstride,
			 gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 4;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  ((guint32 *)obuf)[x] = (b << 24) | (g << 16) | (r << 8);
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_8880_br (GdkImage *image,
			 gint x0, gint y0, gint width, gint height,
			 guchar *buf, int rowstride,
			 gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf;
  gint bpl;
  guchar *bptr, *bp2;
  int r, g, b;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * 4;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  ((guint32 *)obuf)[x] = (b << 16) | (g << 8) | r;
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* Generic truecolor/directcolor conversion function. Slow, but these
   are oddball modes. */
static void
gdk_rgb_convert_truecolor_lsb (GdkImage *image,
			       gint x0, gint y0, gint width, gint height,
			       guchar *buf, int rowstride,
			       gint x_align, gint y_align,
			       GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  gint r, g, b;
  gint r_right, r_left;
  gint g_right, g_left;
  gint b_right, b_left;
  gint bpp;
  guint32 pixel;
  gint i;

  r_right = 8 - image_info->visual->red_prec;
  r_left = image_info->visual->red_shift;
  g_right = 8 - image_info->visual->green_prec;
  g_left = image_info->visual->green_shift;
  b_right = 8 - image_info->visual->blue_prec;
  b_left = image_info->visual->blue_shift;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * bpp;
  for (y = 0; y < height; y++)
    {
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  pixel = ((r >> r_right) << r_left) |
	    ((g >> g_right) << g_left) |
	    ((b >> b_right) << b_left);
	  for (i = 0; i < bpp; i++)
	    {
	      *obptr++ = pixel & 0xff;
	      pixel >>= 8;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_truecolor_lsb_d (GdkImage *image,
				 gint x0, gint y0, gint width, gint height,
				 guchar *buf, int rowstride,
				 gint x_align, gint y_align,
				 GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  gint r, g, b;
  gint r_right, r_left, r_prec;
  gint g_right, g_left, g_prec;
  gint b_right, b_left, b_prec;
  gint bpp;
  guint32 pixel;
  gint i;
  gint dith;
  gint r1, g1, b1;
  const guchar *dmp;

  r_right = 8 - image_info->visual->red_prec;
  r_left = image_info->visual->red_shift;
  r_prec = image_info->visual->red_prec;
  g_right = 8 - image_info->visual->green_prec;
  g_left = image_info->visual->green_shift;
  g_prec = image_info->visual->green_prec;
  b_right = 8 - image_info->visual->blue_prec;
  b_left = image_info->visual->blue_shift;
  b_prec = image_info->visual->blue_prec;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * bpp;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  dith = dmp[(x_align + x) & (DM_WIDTH - 1)] << 2;
	  r1 = r + (dith >> r_prec);
	  g1 = g + ((252 - dith) >> g_prec);
	  b1 = b + (dith >> b_prec);
	  pixel = (((r1 - (r1 >> r_prec)) >> r_right) << r_left) |
	    (((g1 - (g1 >> g_prec)) >> g_right) << g_left) |
	    (((b1 - (b1 >> b_prec)) >> b_right) << b_left);
	  for (i = 0; i < bpp; i++)
	    {
	      *obptr++ = pixel & 0xff;
	      pixel >>= 8;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_truecolor_msb (GdkImage *image,
			       gint x0, gint y0, gint width, gint height,
			       guchar *buf, int rowstride,
			       gint x_align, gint y_align,
			       GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  gint r, g, b;
  gint r_right, r_left;
  gint g_right, g_left;
  gint b_right, b_left;
  gint bpp;
  guint32 pixel;
  gint shift, shift_init;

  r_right = 8 - image_info->visual->red_prec;
  r_left = image_info->visual->red_shift;
  g_right = 8 - image_info->visual->green_prec;
  g_left = image_info->visual->green_shift;
  b_right = 8 - image_info->visual->blue_prec;
  b_left = image_info->visual->blue_shift;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * bpp;
  shift_init = (bpp - 1) << 3;
  for (y = 0; y < height; y++)
    {
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  pixel = ((r >> r_right) << r_left) |
	    ((g >> g_right) << g_left) |
	    ((b >> b_right) << b_left);
	  for (shift = shift_init; shift >= 0; shift -= 8)
	    {
	      *obptr++ = (pixel >> shift) & 0xff;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_truecolor_msb_d (GdkImage *image,
				 gint x0, gint y0, gint width, gint height,
				 guchar *buf, int rowstride,
				 gint x_align, gint y_align,
				 GdkRgbCmap *cmap)
{
  int x, y;
  guchar *obuf, *obptr;
  gint bpl;
  guchar *bptr, *bp2;
  gint r, g, b;
  gint r_right, r_left, r_prec;
  gint g_right, g_left, g_prec;
  gint b_right, b_left, b_prec;
  gint bpp;
  guint32 pixel;
  gint shift, shift_init;
  gint dith;
  gint r1, g1, b1;
  const guchar *dmp;

  r_right = 8 - image_info->visual->red_prec;
  r_left = image_info->visual->red_shift;
  r_prec = image_info->visual->red_prec;
  g_right = 8 - image_info->visual->green_prec;
  g_left = image_info->visual->green_shift;
  g_prec = image_info->visual->green_prec;
  b_right = 8 - image_info->visual->blue_prec;
  b_left = image_info->visual->blue_shift;
  b_prec = image_info->visual->blue_prec;
  bpp = image_info->bpp;
  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0 * bpp;
  shift_init = (bpp - 1) << 3;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      obptr = obuf;
      bp2 = bptr;
      for (x = 0; x < width; x++)
	{
	  r = bp2[0];
	  g = bp2[1];
	  b = bp2[2];
	  dith = dmp[(x_align + x) & (DM_WIDTH - 1)] << 2;
	  r1 = r + (dith >> r_prec);
	  g1 = g + ((252 - dith) >> g_prec);
	  b1 = b + (dith >> b_prec);
	  pixel = (((r1 - (r1 >> r_prec)) >> r_right) << r_left) |
	    (((g1 - (g1 >> g_prec)) >> g_right) << g_left) |
	    (((b1 - (b1 >> b_prec)) >> b_right) << b_left);
	  for (shift = shift_init; shift >= 0; shift -= 8)
	    {
	      *obptr++ = (pixel >> shift) & 0xff;
	    }
	  bp2 += 3;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* This actually works for depths from 3 to 7 */
static void
gdk_rgb_convert_4 (GdkImage *image,
		   gint x0, gint y0, gint width, gint height,
		   guchar *buf, int rowstride,
		   gint x_align, gint y_align,
		   GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  const guchar *dmp;
  gint dith;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x += 1)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) | 3;
	  obptr[0] = colorcube_d[(((r + dith) & 0x100) >> 2) |
				(((g + 258 - dith) & 0x100) >> 5) |
				(((b + dith) & 0x100) >> 8)];
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* This actually works for depths from 3 to 7 */
static void
gdk_rgb_convert_gray4 (GdkImage *image,
		       gint x0, gint y0, gint width, gint height,
		       guchar *buf, int rowstride,
		       gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  gint shift;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  shift = 9 - image_info->visual->depth;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  obptr[0] = (g + ((b + r) >> 1)) >> shift;
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_gray4_pack (GdkImage *image,
			    gint x0, gint y0, gint width, gint height,
			    guchar *buf, int rowstride,
			    gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  gint shift;
  guchar pix0, pix1;
  /* todo: this is hardcoded to big-endian. Make endian-agile. */

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + (x0 >> 1);
  shift = 9 - image_info->visual->depth;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x += 2)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  pix0 = (g + ((b + r) >> 1)) >> shift;
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  pix1 = (g + ((b + r) >> 1)) >> shift;
	  obptr[0] = (pix0 << 4) | pix1;
	  obptr++;
	}
      if (width & 1)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  pix0 = (g + ((b + r) >> 1)) >> shift;
	  obptr[0] = (pix0 << 4);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

/* This actually works for depths from 3 to 7 */
static void
gdk_rgb_convert_gray4_d (GdkImage *image,
		       gint x0, gint y0, gint width, gint height,
		       guchar *buf, int rowstride,
		       gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  const guchar *dmp;
  gint prec, right;
  gint gray;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + x0;
  prec = image_info->visual->depth;
  right = 8 - prec;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) >> prec;
	  obptr[0] = (gray - (gray >> prec)) >> right;
	  obptr++;
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_gray4_d_pack (GdkImage *image,
			      gint x0, gint y0, gint width, gint height,
			      guchar *buf, int rowstride,
			      gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  const guchar *dmp;
  gint prec, right;
  gint gray;
  guchar pix0, pix1;
  /* todo: this is hardcoded to big-endian. Make endian-agile. */

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + (x0 >> 1);
  prec = image_info->visual->depth;
  right = 8 - prec;
  for (y = 0; y < height; y++)
    {
      bp2 = bptr;
      obptr = obuf;
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      for (x = 0; x < width; x += 2)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x) & (DM_WIDTH - 1)] << 2) >> prec;
	  pix0 = (gray - (gray >> prec)) >> right;
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x + 1) & (DM_WIDTH - 1)] << 2) >> prec;
	  pix1 = (gray - (gray >> prec)) >> right;
	  obptr[0] = (pix0 << 4) | pix1;
	  obptr++;
	}
      if (width & 1)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  gray = (g + ((b + r) >> 1)) >> 1;
	  gray += (dmp[(x_align + x + 1) & (DM_WIDTH - 1)] << 2) >> prec;
	  pix0 = (gray - (gray >> prec)) >> right;
	  obptr[0] = (pix0 << 4);
	}
      bptr += rowstride;
      obuf += bpl;
    }
}

static void
gdk_rgb_convert_1 (GdkImage *image,
		   gint x0, gint y0, gint width, gint height,
		   guchar *buf, int rowstride,
		   gint x_align, gint y_align,
		   GdkRgbCmap *cmap)
{
  int x, y;
  gint bpl;
  guchar *obuf, *obptr;
  guchar *bptr, *bp2;
  gint r, g, b;
  const guchar *dmp;
  gint dith;
  guchar byte;

  bptr = buf;
  bpl = image->bpl;
  obuf = ((guchar *)image->mem) + y0 * bpl + (x0 >> 3);
  byte = 0; /* unnecessary, but it keeps gcc from complaining */
  for (y = 0; y < height; y++)
    {
      dmp = DM[(y_align + y) & (DM_HEIGHT - 1)];
      bp2 = bptr;
      obptr = obuf;
      for (x = 0; x < width; x++)
	{
	  r = *bp2++;
	  g = *bp2++;
	  b = *bp2++;
	  dith = (dmp[(x_align + x) & (DM_WIDTH - 1)] << 4) | 4;
	  byte += byte + (r + g + g + b + dith > 1020);
	  if ((x & 7) == 7)
	    {
	      obptr[0] = byte;
	      obptr++;
	    }
	}
      if (x & 7)
	obptr[0] = byte << (8 - (x & 7));
      bptr += rowstride;
      obuf += bpl;
    }
}

/* Returns a pointer to the stage buffer. */
static guchar *
gdk_rgb_ensure_stage (void)
{
  if (image_info->stage_buf == NULL)
    image_info->stage_buf = g_malloc (REGION_HEIGHT * STAGE_ROWSTRIDE);
  return image_info->stage_buf;
}

/* This is slow. Speed me up, please. */
static void
gdk_rgb_32_to_stage (guchar *buf, gint rowstride, gint width, gint height)
{
  gint x, y;
  guchar *pi_start, *po_start;
  guchar *pi, *po;

  pi_start = buf;
  po_start = gdk_rgb_ensure_stage ();
  for (y = 0; y < height; y++)
    {
      pi = pi_start;
      po = po_start;
      for (x = 0; x < width; x++)
	{
	  *po++ = *pi++;
	  *po++ = *pi++;
	  *po++ = *pi++;
	  pi++;
	}
      pi_start += rowstride;
      po_start += STAGE_ROWSTRIDE;
    }
}

/* Generic 32bit RGB conversion function - convert to 24bit packed, then
   go from there. */
static void
gdk_rgb_convert_32_generic (GdkImage *image,
			    gint x0, gint y0, gint width, gint height,
			    guchar *buf, gint rowstride,
			    gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  gdk_rgb_32_to_stage (buf, rowstride, width, height);

  (*image_info->conv) (image, x0, y0, width, height,
		       image_info->stage_buf, STAGE_ROWSTRIDE,
		       x_align, y_align, cmap);
}

/* Generic 32bit RGB conversion function - convert to 24bit packed, then
   go from there. */
static void
gdk_rgb_convert_32_generic_d (GdkImage *image,
			      gint x0, gint y0, gint width, gint height,
			      guchar *buf, gint rowstride,
			      gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  gdk_rgb_32_to_stage (buf, rowstride, width, height);

  (*image_info->conv_d) (image, x0, y0, width, height,
			 image_info->stage_buf, STAGE_ROWSTRIDE,
			 x_align, y_align, cmap);
}

/* This is slow. Speed me up, please. */
static void
gdk_rgb_gray_to_stage (guchar *buf, gint rowstride, gint width, gint height)
{
  gint x, y;
  guchar *pi_start, *po_start;
  guchar *pi, *po;
  guchar gray;

  pi_start = buf;
  po_start = gdk_rgb_ensure_stage ();
  for (y = 0; y < height; y++)
    {
      pi = pi_start;
      po = po_start;
      for (x = 0; x < width; x++)
	{
	  gray = *pi++;
	  *po++ = gray;
	  *po++ = gray;
	  *po++ = gray;
	}
      pi_start += rowstride;
      po_start += STAGE_ROWSTRIDE;
    }
}

/* Generic gray conversion function - convert to 24bit packed, then go
   from there. */
static void
gdk_rgb_convert_gray_generic (GdkImage *image,
			      gint x0, gint y0, gint width, gint height,
			      guchar *buf, gint rowstride,
			      gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  gdk_rgb_gray_to_stage (buf, rowstride, width, height);

  (*image_info->conv) (image, x0, y0, width, height,
		       image_info->stage_buf, STAGE_ROWSTRIDE,
		       x_align, y_align, cmap);
}

static void
gdk_rgb_convert_gray_generic_d (GdkImage *image,
				gint x0, gint y0, gint width, gint height,
				guchar *buf, gint rowstride,
				gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  gdk_rgb_gray_to_stage (buf, rowstride, width, height);

  (*image_info->conv_d) (image, x0, y0, width, height,
			 image_info->stage_buf, STAGE_ROWSTRIDE,
			 x_align, y_align, cmap);
}

/* Render grayscale using indexed method. */
static void
gdk_rgb_convert_gray_cmap (GdkImage *image,
			   gint x0, gint y0, gint width, gint height,
			   guchar *buf, gint rowstride,
			   gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  (*image_info->conv_indexed) (image, x0, y0, width, height,
			       buf, rowstride,
			       x_align, y_align, image_info->gray_cmap);
}

#if 0
static void
gdk_rgb_convert_gray_cmap_d (GdkImage *image,
				gint x0, gint y0, gint width, gint height,
				guchar *buf, gint rowstride,
				gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  (*image_info->conv_indexed_d) (image, x0, y0, width, height,
				 buf, rowstride,
				 x_align, y_align, image_info->gray_cmap);
}
#endif

/* This is slow. Speed me up, please. */
static void
gdk_rgb_indexed_to_stage (guchar *buf, gint rowstride, gint width, gint height,
			  GdkRgbCmap *cmap)
{
  gint x, y;
  guchar *pi_start, *po_start;
  guchar *pi, *po;
  gint rgb;

  pi_start = buf;
  po_start = gdk_rgb_ensure_stage ();
  for (y = 0; y < height; y++)
    {
      pi = pi_start;
      po = po_start;
      for (x = 0; x < width; x++)
	{
	  rgb = cmap->colors[*pi++];
	  *po++ = rgb >> 16;
	  *po++ = (rgb >> 8) & 0xff;
	  *po++ = rgb & 0xff;
	}
      pi_start += rowstride;
      po_start += STAGE_ROWSTRIDE;
    }
}

/* Generic gray conversion function - convert to 24bit packed, then go
   from there. */
static void
gdk_rgb_convert_indexed_generic (GdkImage *image,
				 gint x0, gint y0, gint width, gint height,
				 guchar *buf, gint rowstride,
				 gint x_align, gint y_align, GdkRgbCmap *cmap)
{
  gdk_rgb_indexed_to_stage (buf, rowstride, width, height, cmap);

  (*image_info->conv) (image, x0, y0, width, height,
		       image_info->stage_buf, STAGE_ROWSTRIDE,
		       x_align, y_align, cmap);
}

static void
gdk_rgb_convert_indexed_generic_d (GdkImage *image,
				   gint x0, gint y0, gint width, gint height,
				   guchar *buf, gint rowstride,
				   gint x_align, gint y_align,
				   GdkRgbCmap *cmap)
{
  gdk_rgb_indexed_to_stage (buf, rowstride, width, height, cmap);

  (*image_info->conv_d) (image, x0, y0, width, height,
			 image_info->stage_buf, STAGE_ROWSTRIDE,
			 x_align, y_align, cmap);
}

/* Select a conversion function based on the visual and a
   representative image. */
static void
gdk_rgb_select_conv (GdkImage *image)
{
  GdkByteOrder byte_order;
  gint depth, bpp, byterev;
  GdkVisualType vtype;
  guint32 red_mask, green_mask, blue_mask;
  GdkRgbConvFunc conv, conv_d;
  GdkRgbConvFunc conv_32, conv_32_d;
  GdkRgbConvFunc conv_gray, conv_gray_d;
  GdkRgbConvFunc conv_indexed, conv_indexed_d;
  gboolean mask_rgb, mask_bgr;

  depth = image_info->visual->depth;
  bpp = ((GdkImagePrivate *)image)->ximage->bits_per_pixel;

  byte_order = image->byte_order;
  if (gdk_rgb_verbose)
    g_print ("Chose visual 0x%x, image bpp=%d, %s first\n",
	     (gint)(((GdkVisualPrivate *)image_info->visual)->xvisual->visualid),
	     bpp, byte_order == GDK_LSB_FIRST ? "lsb" : "msb");

#if G_BYTE_ORDER == G_BIG_ENDIAN
  byterev = (byte_order == GDK_LSB_FIRST);
#else
  byterev = (byte_order == GDK_MSB_FIRST);
#endif

  vtype = image_info->visual->type;
  if (vtype == GDK_VISUAL_DIRECT_COLOR)
    vtype = GDK_VISUAL_TRUE_COLOR;

  red_mask = image_info->visual->red_mask;
  green_mask = image_info->visual->green_mask;
  blue_mask = image_info->visual->blue_mask;

  mask_rgb = red_mask == 0xff0000 && green_mask == 0xff00 && blue_mask == 0xff;
  mask_bgr = red_mask == 0xff && green_mask == 0xff00 && blue_mask == 0xff0000;

  conv = NULL;
  conv_d = NULL;

  conv_32 = gdk_rgb_convert_32_generic;
  conv_32_d = gdk_rgb_convert_32_generic_d;

  conv_gray = gdk_rgb_convert_gray_generic;
  conv_gray_d = gdk_rgb_convert_gray_generic_d;

  conv_indexed = gdk_rgb_convert_indexed_generic;
  conv_indexed_d = gdk_rgb_convert_indexed_generic_d;

  image_info->dith_default = FALSE;

  if (image_info->bitmap)
    conv = gdk_rgb_convert_1;
  else if (bpp == 16 && depth == 16 && !byterev &&
      red_mask == 0xf800 && green_mask == 0x7e0 && blue_mask == 0x1f)
    {
      conv = gdk_rgb_convert_565;
      conv_d = gdk_rgb_convert_565_d;
      conv_gray = gdk_rgb_convert_565_gray;
      /* Broken, need to shift to GdkPixbuf/GtkImage */
      /* gdk_rgb_preprocess_dm_565 (); */
    }
  else if (bpp == 16 && depth == 16 &&
	   vtype == GDK_VISUAL_TRUE_COLOR && byterev &&
      red_mask == 0xf800 && green_mask == 0x7e0 && blue_mask == 0x1f)
    conv = gdk_rgb_convert_565_br;

  else if (bpp == 16 && depth == 15 &&
	   vtype == GDK_VISUAL_TRUE_COLOR && !byterev &&
      red_mask == 0x7c00 && green_mask == 0x3e0 && blue_mask == 0x1f)
    conv = gdk_rgb_convert_555;

  else if (bpp == 16 && depth == 15 &&
	   vtype == GDK_VISUAL_TRUE_COLOR && byterev &&
      red_mask == 0x7c00 && green_mask == 0x3e0 && blue_mask == 0x1f)
    conv = gdk_rgb_convert_555_br;

  /* I'm not 100% sure about the 24bpp tests - but testing will show*/
  else if (bpp == 24 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   ((mask_rgb && byte_order == GDK_LSB_FIRST) ||
	    (mask_bgr && byte_order == GDK_MSB_FIRST)))
    conv = gdk_rgb_convert_888_lsb;
  else if (bpp == 24 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   ((mask_rgb && byte_order == GDK_MSB_FIRST) ||
	    (mask_bgr && byte_order == GDK_LSB_FIRST)))
    conv = gdk_rgb_convert_888_msb;
#if G_BYTE_ORDER == G_BIG_ENDIAN
  else if (bpp == 32 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   (mask_rgb && byte_order == GDK_LSB_FIRST))
    conv = gdk_rgb_convert_0888_br;
  else if (bpp == 32 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   (mask_rgb && byte_order == GDK_MSB_FIRST))
    conv = gdk_rgb_convert_0888;
  else if (bpp == 32 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   (mask_bgr && byte_order == GDK_MSB_FIRST))
    conv = gdk_rgb_convert_8880_br;
#else
  else if (bpp == 32 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   (mask_rgb && byte_order == GDK_MSB_FIRST))
    conv = gdk_rgb_convert_0888_br;
  else if (bpp == 32 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   (mask_rgb && byte_order == GDK_LSB_FIRST))
    conv = gdk_rgb_convert_0888;
  else if (bpp == 32 && depth == 24 && vtype == GDK_VISUAL_TRUE_COLOR &&
	   (mask_bgr && byte_order == GDK_LSB_FIRST))
    conv = gdk_rgb_convert_8880_br;
#endif

  else if (vtype == GDK_VISUAL_TRUE_COLOR && byte_order == GDK_LSB_FIRST)
    {
      conv = gdk_rgb_convert_truecolor_lsb;
      conv_d = gdk_rgb_convert_truecolor_lsb_d;
    }
  else if (vtype == GDK_VISUAL_TRUE_COLOR && byte_order == GDK_MSB_FIRST)
    {
      conv = gdk_rgb_convert_truecolor_msb;
      conv_d = gdk_rgb_convert_truecolor_msb_d;
    }
  else if (bpp == 8 && depth == 8 && (vtype == GDK_VISUAL_PSEUDO_COLOR
#ifdef ENABLE_GRAYSCALE
				      || vtype == GDK_VISUAL_GRAYSCALE
#endif
				      ))
    {
      image_info->dith_default = TRUE;
      conv = gdk_rgb_convert_8;
      if (vtype != GDK_VISUAL_GRAYSCALE)
	{
	  if (image_info->nred_shades == 6 &&
	      image_info->ngreen_shades == 6 &&
	      image_info->nblue_shades == 6)
	    conv_d = gdk_rgb_convert_8_d666;
	  else
	    conv_d = gdk_rgb_convert_8_d;
	}
      conv_indexed = gdk_rgb_convert_8_indexed;
      conv_gray = gdk_rgb_convert_gray_cmap;
    }
  else if (bpp == 8 && depth == 8 && (vtype == GDK_VISUAL_STATIC_GRAY
#ifdef not_ENABLE_GRAYSCALE
				      || vtype == GDK_VISUAL_GRAYSCALE
#endif
				      ))
    {
      conv = gdk_rgb_convert_gray8;
      conv_gray = gdk_rgb_convert_gray8_gray;
    }
  else if (bpp == 8 && depth < 8 && depth >= 2 &&
	   (vtype == GDK_VISUAL_STATIC_GRAY
	    || vtype == GDK_VISUAL_GRAYSCALE))
    {
      conv = gdk_rgb_convert_gray4;
      conv_d = gdk_rgb_convert_gray4_d;
    }
  else if (bpp == 8 && depth < 8 && depth >= 3)
    {
      conv = gdk_rgb_convert_4;
    }
  else if (bpp == 4 && depth <= 4 && depth >= 2 &&
	   (vtype == GDK_VISUAL_STATIC_GRAY
	    || vtype == GDK_VISUAL_GRAYSCALE))
    {
      conv = gdk_rgb_convert_gray4_pack;
      conv_d = gdk_rgb_convert_gray4_d_pack;
    }

  if (!conv)
    {
      g_warning ("Visual type=%s depth=%d, image bpp=%d, %s first\n"
		 "is not supported by GdkRGB. Please submit a bug report\n"
		 "with the above values to bugzilla.gnome.org",
		 visual_names[vtype], depth, bpp,
		 byte_order == GDK_LSB_FIRST ? "lsb" : "msb");
      exit (1);
    }
  
  if (conv_d == NULL)
    conv_d = conv;

  image_info->conv = conv;
  image_info->conv_d = conv_d;

  image_info->conv_32 = conv_32;
  image_info->conv_32_d = conv_32_d;

  image_info->conv_gray = conv_gray;
  image_info->conv_gray_d = conv_gray_d;

  image_info->conv_indexed = conv_indexed;
  image_info->conv_indexed_d = conv_indexed_d;
}

static gint horiz_idx;
static gint horiz_y = REGION_HEIGHT;
static gint vert_idx;
static gint vert_x = REGION_WIDTH;
static gint tile_idx;
static gint tile_x = REGION_WIDTH;
static gint tile_y1 = REGION_HEIGHT;
static gint tile_y2 = REGION_HEIGHT;

#ifdef VERBOSE
static gint sincelast;
#endif

/* Defining NO_FLUSH can cause inconsistent screen updates, but is useful
   for performance evaluation. */

#undef NO_FLUSH

static gint
gdk_rgb_alloc_scratch_image ()
{
  if (static_image_idx == N_REGIONS)
    {
#ifndef NO_FLUSH
      gdk_flush ();
#endif
#ifdef VERBOSE
      g_print ("flush, %d puts since last flush\n", sincelast);
      sincelast = 0;
#endif
      static_image_idx = 0;

      /* Mark all regions that we might be filling in as completely
       * full, to force new tiles to be allocated for subsequent
       * images
       */
      horiz_y = REGION_HEIGHT;
      vert_x = REGION_WIDTH;
      tile_x = REGION_WIDTH;
      tile_y1 = tile_y2 = REGION_HEIGHT;
    }
  return static_image_idx++;
}

static GdkImage *
gdk_rgb_alloc_scratch (gint width, gint height, gint *x0, gint *y0)
{
  GdkImage *image;
  gint idx;

  if (width >= (REGION_WIDTH >> 1))
    {
      if (height >= (REGION_HEIGHT >> 1))
	{
	  idx = gdk_rgb_alloc_scratch_image ();
	  *x0 = 0;
	  *y0 = 0;
	}
      else
	{
	  if (height + horiz_y > REGION_HEIGHT)
	    {
	      horiz_idx = gdk_rgb_alloc_scratch_image ();
	      horiz_y = 0;
	    }
	  idx = horiz_idx;
	  *x0 = 0;
	  *y0 = horiz_y;
	  horiz_y += height;
	}
    }
  else
    {
      if (height >= (REGION_HEIGHT >> 1))
	{
	  if (width + vert_x > REGION_WIDTH)
	    {
	      vert_idx = gdk_rgb_alloc_scratch_image ();
	      vert_x = 0;
	    }
	  idx = vert_idx;
	  *x0 = vert_x;
	  *y0 = 0;
	  /* using 3 and -4 would be slightly more efficient on 32-bit machines
	     with > 1bpp displays */
	  vert_x += (width + 7) & -8;
	}
      else
	{
	  if (width + tile_x > REGION_WIDTH)
	    {
	      tile_y1 = tile_y2;
	      tile_x = 0;
	    }
	  if (height + tile_y1 > REGION_HEIGHT)
	    {
	      tile_idx = gdk_rgb_alloc_scratch_image ();
	      tile_x = 0;
	      tile_y1 = 0;
	      tile_y2 = 0;
	    }
	  if (height + tile_y1 > tile_y2)
	    tile_y2 = height + tile_y1;
	  idx = tile_idx;
	  *x0 = tile_x;
	  *y0 = tile_y1;
	  tile_x += (width + 7) & -8;
	}
    }
  image = static_image[idx * static_n_images / N_REGIONS];
  *x0 += REGION_WIDTH * (idx % (N_REGIONS / static_n_images));
#ifdef VERBOSE
  g_print ("index %d, x %d, y %d (%d x %d)\n", idx, *x0, *y0, width, height);
  sincelast++;
#endif
  return image;
}

static void
gdk_draw_rgb_image_core (GdkDrawable *drawable,
			 GdkGC *gc,
			 gint x,
			 gint y,
			 gint width,
			 gint height,
			 guchar *buf,
			 gint pixstride,
			 gint rowstride,
			 GdkRgbConvFunc conv,
			 GdkRgbCmap *cmap,
			 gint xdith,
			 gint ydith)
{
  gint y0, x0;
  gint xs0, ys0;
  GdkImage *image;
  gint width1, height1;
  guchar *buf_ptr;

  if (image_info->bitmap)
    {
      if (image_info->own_gc == NULL)
	{
	  GdkColor color;

	  image_info->own_gc = gdk_gc_new (drawable);
	  gdk_color_white (image_info->cmap, &color);
	  gdk_gc_set_foreground (image_info->own_gc, &color);
	  gdk_color_black (image_info->cmap, &color);
	  gdk_gc_set_background (image_info->own_gc, &color);
	}
      gc = image_info->own_gc;
    }
  for (y0 = 0; y0 < height; y0 += REGION_HEIGHT)
    {
      height1 = MIN (height - y0, REGION_HEIGHT);
      for (x0 = 0; x0 < width; x0 += REGION_WIDTH)
	{
	  width1 = MIN (width - x0, REGION_WIDTH);
	  buf_ptr = buf + y0 * rowstride + x0 * pixstride;

	  image = gdk_rgb_alloc_scratch (width1, height1, &xs0, &ys0);

	  conv (image, xs0, ys0, width1, height1, buf_ptr, rowstride,
		x + x0 + xdith, y + y0 + ydith, cmap);

#ifndef DONT_ACTUALLY_DRAW
	  gdk_draw_image (drawable, gc,
			  image, xs0, ys0, x + x0, y + y0, width1, height1);
#endif
	}
    }
}


void
gdk_draw_rgb_image (GdkDrawable *drawable,
		    GdkGC *gc,
		    gint x,
		    gint y,
		    gint width,
		    gint height,
		    GdkRgbDither dith,
		    guchar *rgb_buf,
		    gint rowstride)
{
  if (dith == GDK_RGB_DITHER_NONE || (dith == GDK_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     rgb_buf, 3, rowstride, image_info->conv, NULL,
			     0, 0);
  else
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     rgb_buf, 3, rowstride, image_info->conv_d, NULL,
			     0, 0);
}

void
gdk_draw_rgb_image_dithalign (GdkDrawable *drawable,
			      GdkGC *gc,
			      gint x,
			      gint y,
			      gint width,
			      gint height,
			      GdkRgbDither dith,
			      guchar *rgb_buf,
			      gint rowstride,
			      gint xdith,
			      gint ydith)
{
  if (dith == GDK_RGB_DITHER_NONE || (dith == GDK_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     rgb_buf, 3, rowstride, image_info->conv, NULL,
			     xdith, ydith);
  else
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     rgb_buf, 3, rowstride, image_info->conv_d, NULL,
			     xdith, ydith);
}

void
gdk_draw_rgb_32_image (GdkDrawable *drawable,
		       GdkGC *gc,
		       gint x,
		       gint y,
		       gint width,
		       gint height,
		       GdkRgbDither dith,
		       guchar *buf,
		       gint rowstride)
{
  if (dith == GDK_RGB_DITHER_NONE || (dith == GDK_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     buf, 4, rowstride,
			     image_info->conv_32, NULL, 0, 0);
  else
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     buf, 4, rowstride,
			     image_info->conv_32_d, NULL, 0, 0);
}

static void
gdk_rgb_make_gray_cmap (GdkRgbInfo *info)
{
  guint32 rgb[256];
  gint i;

  for (i = 0; i < 256; i++)
    rgb[i] = (i << 16)  | (i << 8) | i;
  info->gray_cmap = gdk_rgb_cmap_new (rgb, 256);
}

void
gdk_draw_gray_image (GdkDrawable *drawable,
		     GdkGC *gc,
		     gint x,
		     gint y,
		     gint width,
		     gint height,
		     GdkRgbDither dith,
		     guchar *buf,
		     gint rowstride)
{
  if (image_info->bpp == 1 &&
      image_info->gray_cmap == NULL &&
      (image_info->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       image_info->visual->type == GDK_VISUAL_GRAYSCALE))
    gdk_rgb_make_gray_cmap (image_info);
  
  if (dith == GDK_RGB_DITHER_NONE || (dith == GDK_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     buf, 1, rowstride,
			     image_info->conv_gray, NULL, 0, 0);
  else
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     buf, 1, rowstride,
			     image_info->conv_gray_d, NULL, 0, 0);
}

GdkRgbCmap *
gdk_rgb_cmap_new (guint32 *colors, gint n_colors)
{
  GdkRgbCmap *cmap;
  int i, j;
  guint32 rgb;

  g_return_val_if_fail (n_colors >= 0, NULL);
  g_return_val_if_fail (n_colors <= 256, NULL);
  cmap = g_new (GdkRgbCmap, 1);
  memcpy (cmap->colors, colors, n_colors * sizeof(guint32));
  if (image_info->bpp == 1 &&
      (image_info->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       image_info->visual->type == GDK_VISUAL_GRAYSCALE))
    for (i = 0; i < n_colors; i++)
      {
	rgb = colors[i];
	j = ((rgb & 0xf00000) >> 12) |
		   ((rgb & 0xf000) >> 8) |
		   ((rgb & 0xf0) >> 4);
#ifdef VERBOSE
	g_print ("%d %x %x %d\n", i, j, colorcube[j]);
#endif
	cmap->lut[i] = colorcube[j];
      }
  return cmap;
}

void
gdk_rgb_cmap_free (GdkRgbCmap *cmap)
{
  g_free (cmap);
}

void
gdk_draw_indexed_image (GdkDrawable *drawable,
			GdkGC *gc,
			gint x,
			gint y,
			gint width,
			gint height,
			GdkRgbDither dith,
			guchar *buf,
			gint rowstride,
			GdkRgbCmap *cmap)
{
  if (dith == GDK_RGB_DITHER_NONE || (dith == GDK_RGB_DITHER_NORMAL &&
				      !image_info->dith_default))
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     buf, 1, rowstride,
			     image_info->conv_indexed, cmap, 0, 0);
  else
    gdk_draw_rgb_image_core (drawable, gc, x, y, width, height,
			     buf, 1, rowstride,
			     image_info->conv_indexed_d, cmap, 0, 0);
}

gboolean
gdk_rgb_ditherable (void)
{
  return (image_info->conv != image_info->conv_d);
}

GdkColormap *
gdk_rgb_get_cmap (void)
{
  gdk_rgb_init ();
  return image_info->cmap;
}

GdkVisual *
gdk_rgb_get_visual (void)
{
  gdk_rgb_init ();
  return image_info->visual;
}
