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
 * Copyright 2013 RemyAWM <remyxxl@gmail.com>
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_CHANT_PROPERTIES
gegl_chant_color(color, _("Color"), "red", _("Fill color"))
gegl_chant_int_ui(type, _("Fill type"), 0, 2, 0, 0, 2, 1.0, _("0 - soft; 1 - hard; 2 - over"))
#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE       "fx-color.c"

#include "gegl-chant.h"

static void prepare (GeglOperation *operation)
{
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static gboolean
process (GeglOperation       *operation,
		 void                *input,
		 void                *output,
         glong                samples,
         const GeglRectangle *result,
         gint                 level)
{
	GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
	gfloat      line_color[4];

	gfloat *in  = input;
	gfloat *out = output;

	gegl_color_get_pixel (o->color, babl_format ("RGBA float"), line_color);

	  while (samples--)
	    {
		  if(in[3]!=0)
	      out[0] =  line_color[0];
	      out[1] =  line_color[1];
	      out[2] =  line_color[2];

	      out[3] = in[3];

	      in += 4;
	      out+= 4;
	    }

  return  TRUE;
}


static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationPointFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  filter_class->process = process;
  operation_class->prepare = prepare;

  gegl_operation_class_set_keys (operation_class,
    "name"       , "gegl:fx-color",
    "categories" , "fx:fill",
    "description", _("Fill color"),
    NULL);
}

#endif

