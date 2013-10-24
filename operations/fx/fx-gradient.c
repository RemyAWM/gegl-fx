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
	//Add props ...
#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE       "fx-gradient.c"

#include "gegl-chant.h"

#include "graph/gegl-node.h"
#include <math.h>

static void prepare (GeglOperation *operation)
{
  const Babl *format = gegl_operation_get_source_format (operation, "input");
  const Babl *format_aux = gegl_operation_get_source_format (operation, "aux");

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
  gegl_operation_set_format (operation, "aux", format_aux);
}

static GeglNode *
gegl_gradient_detect (GeglOperation *operation,
                  gint           x,
                  gint           y)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
  GeglNode   *input_node, *aux_node;

  input_node = gegl_operation_get_source_node (operation, "input");
  aux_node = gegl_operation_get_source_node (operation, "aux");

  if (input_node && aux_node)
  {
	  GeglRectangle  in_box = gegl_node_get_bounding_box (input_node);

	  return gegl_operation_detect (input_node->operation,
                                  x - floor (in_box.x),
                                  y - floor (in_box.y));
  }

  return operation->node;
}

static GeglRectangle
gegl_gradient_get_bounding_box (GeglOperation *operation)
{
  GeglChantO    *o = GEGL_CHANT_PROPERTIES (operation);
  GeglRectangle *in_rect = gegl_operation_source_get_bounding_box (operation, "input");
  GeglRectangle  result  = { 0, 0, 0, 0 };

  if (!in_rect)
    return result;

  result.x = o->x;
  result.y = o->y;
  result.width  = o->width;
  result.height = o->height;

  return result;
}

