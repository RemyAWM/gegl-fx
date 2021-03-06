/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * This plugin is used for removing the red-eye effect
 * that occurs in flash photos.
 *
 * Based on a GIMP 1.2 Perl plugin by Geoff Kuenning
 *
 * Copyright (C) 2004 Robert Merkel <robert.merkel@benambra.org>
 * Copyright (C) 2006 Andreas Røsdal <andrearo@stud.ntnu.no>
 * Copyright (C) 2011 Robert Sasu <sasu.robert@gmail.com>
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (threshold, _("Threshold"),
                   0.0, 0.8, 0.4,
                   _("Red eye threshold"))

#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE "red-eye-removal.c"

#include "gegl-chant.h"
#include <stdio.h>
#include <math.h>

#define RED_FACTOR    0.5133333
#define GREEN_FACTOR  1
#define BLUE_FACTOR   0.1933333

static void
prepare (GeglOperation *operation)
{
  gegl_operation_set_format (operation, "input",
                             babl_format ("R'G'B'A float"));
  gegl_operation_set_format (operation, "output",
                             babl_format ("R'G'B'A float"));
}

static void
red_eye_reduction (gfloat *buf,
                   gfloat  threshold)
{
  gfloat adjusted_red       = buf[0] * RED_FACTOR;
  gfloat adjusted_green     = buf[1] * GREEN_FACTOR;
  gfloat adjusted_blue      = buf[2] * BLUE_FACTOR;
  gfloat adjusted_threshold = (threshold - 0.4) * 2;
  gfloat tmp;

  if (adjusted_red >= adjusted_green - adjusted_threshold &&
      adjusted_red >= adjusted_blue  - adjusted_threshold)
    {
      tmp = (gdouble) (adjusted_green + adjusted_blue) / (2.0 * RED_FACTOR);
      buf[0] = CLAMP (tmp, 0.0, 1.0);
    }
  /* Otherwise, leave the red channel alone */
}

static gboolean
process (GeglOperation       *operation,
         void                *in_buf,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi,
         gint                 level)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);

  gfloat *dest = out_buf;
  glong   i;

  /*
   * Initialize the dest buffer to the input buffer
   * (we only want to change the red component)
   */
  memcpy (out_buf, in_buf, sizeof (gfloat) * 4 * n_pixels);

  for (i = n_pixels; i > 0; i--)
    {
      red_eye_reduction (dest, o->threshold);
      dest += 4;
    }

  return TRUE;
}

static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass            *operation_class;
  GeglOperationPointFilterClass *point_filter_class;

  operation_class    = GEGL_OPERATION_CLASS (klass);
  point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  operation_class->prepare    = prepare;
  point_filter_class->process = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:red-eye-removal",
    "categories",  "enhance",
    "description", _("Remove the red eye effect caused by camera flashes"),
    NULL);
}

#endif
