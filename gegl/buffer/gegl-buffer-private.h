/* This file is part of GEGL.
 * ck
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */

#ifndef __GEGL_BUFFER_PRIVATE_H__
#define __GEGL_BUFFER_PRIVATE_H__

#include "gegl-buffer.h"
#include "gegl-tile-handler.h"

#define GEGL_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEGL_TYPE_BUFFER, GeglBufferClass))
#define GEGL_IS_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEGL_TYPE_BUFFER))
#define GEGL_IS_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEGL_TYPE_BUFFER))
#define GEGL_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEGL_TYPE_BUFFER, GeglBufferClass))

struct _GeglBuffer
{
  GeglTileHandler   parent_instance; /* which is a GeglTileHandler which has a
                                        source field which is used for chaining
                                        sub buffers with their anchestors */

  GeglRectangle     extent;        /* the dimensions of the buffer */

  const Babl       *format;  /* the pixel format used for pixels in this
                                buffer */

  gint              shift_x; /* The relative offset of origins compared with */
  gint              shift_y; /* anchestral tile_storage buffer, during            */
                             /* construction relative to immediate source  */

  GeglRectangle     abyss;

  GeglTile         *hot_tile; /* cached tile for speeding up pget/pset (1x1
                                 sized gets/sets)*/

  GeglSampler      *sampler; /* cached sampler for speeding up random
                                access interpolated fetches from the
                                buffer */

  GeglTileStorage  *tile_storage;

  gint              min_x; /* the extent of tile indices that has been */
  gint              min_y; /* produced by _get_tile for this buffer */
  gint              max_x; /* this is used in gegl_buffer_void to narrow */
  gint              max_y; /* down the tiles kill commands are sent for */
  gint              max_z;

  gint              tile_width;
  gint              tile_height;
  gchar            *path;
};

struct _GeglBufferClass
{
  GeglTileHandlerClass parent_class;
};


const GeglRectangle* gegl_buffer_get_abyss  (GeglBuffer           *buffer);

gint                 gegl_buffer_leaks       (void);

void                 gegl_buffer_stats       (void);

void                 gegl_buffer_save        (GeglBuffer          *buffer,
                                              const gchar         *path,
                                              const GeglRectangle *roi);


const gchar         *gegl_swap_dir           (void);


void                 gegl_tile_cache_init    (void);

void                 gegl_tile_cache_destroy (void);

GeglTileBackend    * gegl_buffer_backend     (GeglBuffer *buffer);

#endif
