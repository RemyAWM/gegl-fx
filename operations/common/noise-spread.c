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
 * Copyright (C) 2011 Barak Itkin <lightningismyname@gmail.org>
 *
 * Based on "Spread" (Noise) GIMP plugin
 * Copyright (C) 1997 Brian Degenhardt and Federico Mena Quintero
 *
 * The workshop/whirl-pinch.c and common/pixelise.c were used as
 * templates for this op file.
 */
#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_int  (amount_x, _("Horizontal"),
                 0, 256, 5,
                 _("Horizontal spread amount"))

gegl_chant_int  (amount_y, _("Vertical"),
                 0, 256, 5,
                 _("Vertical spread amount"))

gegl_chant_seed (seed, _("Seed"),
                 _("Random seed"))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE "noise-spread.c"

#include "gegl-chant.h"
#include <math.h>

static inline void
calc_sample_coords (gint  src_x,
                    gint  src_y,
                    gint  amount_x,
                    gint  amount_y,
                    gint  seed,
                    gint *x,
                    gint *y)
{
  gdouble angle;
  gint    xdist, ydist;

  /* get random angle, x distance, and y distance */
  xdist = amount_x > 0 ? gegl_random_int_range (seed, src_x, src_y, 0, 0,
                                                -amount_x, amount_x + 1) : 0;
  ydist = amount_y > 0 ? gegl_random_int_range (seed, src_x, src_y, 0, 1,
                                                -amount_y, amount_y + 1) : 0;
  angle = gegl_random_float_range (seed, src_x, src_y, 0, 2, -G_PI, G_PI);

  *x = src_x + floor (sin (angle) * xdist);
  *y = src_y + floor (cos (angle) * ydist);
}


static void
prepare (GeglOperation *operation)
{
  GeglChantO              *o       = GEGL_CHANT_PROPERTIES (operation);
  GeglOperationAreaFilter *op_area = GEGL_OPERATION_AREA_FILTER (operation);
  const Babl              *format;

  op_area->left   =
  op_area->right  = (o->amount_x + 1) / 2;
  op_area->top    =
  op_area->bottom = (o->amount_y + 1) / 2;

  format = gegl_operation_get_source_format (operation, "input");

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglChantO         *o;
  const Babl         *format;
  gint                bpp;
  GeglBufferIterator *gi;
  gint                amount_x;
  gint                amount_y;

  o = GEGL_CHANT_PROPERTIES (operation);

  amount_x = (o->amount_x + 1) / 2;
  amount_y = (o->amount_y + 1) / 2;

  format = gegl_operation_get_source_format (operation, "input");
  bpp = babl_format_get_bytes_per_pixel (format);

  gi = gegl_buffer_iterator_new (output, result, 0, format,
                                 GEGL_BUFFER_WRITE, GEGL_ABYSS_CLAMP);

  while (gegl_buffer_iterator_next (gi))
    {
      gchar        *data = gi->data[0];
      GeglRectangle roi  = gi->roi[0];
      gint          i, j;

      for (j = roi.y; j < roi.y + roi.height ; j++)
        for (i = roi.x; i < roi.x + roi.width ; i++)
          {
            gint x, y;

            calc_sample_coords (i, j, amount_x, amount_y, o->seed, &x, &y);

            gegl_buffer_sample (input, x, y, NULL, data, format,
                                GEGL_SAMPLER_NEAREST, GEGL_ABYSS_CLAMP);
            data += bpp;
          }
    }

  return TRUE;
}

static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->prepare = prepare;
  filter_class->process    = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:noise-spread",
    "categories",  "noise",
    "description", _("Move pixels around randomly"),
    NULL);
}

#endif
