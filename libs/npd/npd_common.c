/*
 * This file is part of N-point image deformation library.
 *
 * N-point image deformation library is free software: you can
 * redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * N-point image deformation library is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with N-point image deformation library.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2013 Marek Dvoroznak <dvoromar@gmail.com>
 */

#include "npd_common.h"
#include "npd_math.h"
#include <math.h>
#include <glib.h>
#include <glib/gprintf.h>

gint  npd_int_sort_function_descending (gconstpointer a,
                                        gconstpointer b);

void
npd_init_model (NPDModel *model)
{
  NPDHiddenModel *hidden_model;
  GArray         *control_points;
  
  /* init hidden model */
  hidden_model        = g_new (NPDHiddenModel, 1);
  model->hidden_model = hidden_model;
  hidden_model->ASAP                      = FALSE;
  hidden_model->MLS_weights               = FALSE;
  hidden_model->MLS_weights_alpha         = 1;
  hidden_model->num_of_bones              = 0;
  hidden_model->num_of_overlapping_points = 0;

  /* init control points */
  control_points        = g_array_new (FALSE, FALSE, sizeof (NPDControlPoint));
  model->control_points = control_points;
  model->control_point_radius             = 6;
  model->control_points_visible           = TRUE;

  /* init other */
  model->mesh_visible                     = TRUE;
  model->texture_visible                  = TRUE;
}

void
npd_destroy_hidden_model (NPDHiddenModel *hm)
{
  gint i;
  for (i = 0; i < hm->num_of_overlapping_points; i++)
    {
      g_free (hm->list_of_overlapping_points[i].points);
    }

  g_free (hm->list_of_overlapping_points);

  for (i = 0; i < hm->num_of_bones; i++)
    {
      g_free (hm->current_bones[i].weights);
      g_free (hm->current_bones[i].points);
      g_free (hm->reference_bones[i].points);
    }
  
  g_free (hm->current_bones);
  g_free (hm->reference_bones);
}

void
npd_destroy_model (NPDModel *model)
{
  /* destroy control points */
  g_array_free (model->control_points, TRUE);

  /* destroy hidden model */
  npd_destroy_hidden_model (model->hidden_model);
  g_free (model->hidden_model);
}

/**
 * Finds nearest (to specified position) overlapping points, creates a new
 * control point at the position of overlapping points and assigns them to the
 * control point.
 * 
 * @param  model
 * @param  coord     specified position
 * @return pointer to a newly created control point or NULL when there already
 * is a control point at the position of nearest overlapping points
 */
NPDControlPoint*
npd_add_control_point (NPDModel *model,
                       NPDPoint *coord)
{
  gint                  num_of_ops, i, closest;
  gfloat                min, current;
  NPDOverlappingPoints *list_of_ops;
  NPDControlPoint       cp;
  NPDPoint             *closest_point;

  list_of_ops = model->hidden_model->list_of_overlapping_points;
  num_of_ops  = model->hidden_model->num_of_overlapping_points;

  /* find closest overlapping points */
  closest = 0;
  min     = npd_SED (coord, list_of_ops[0].representative);

  for (i = 1; i < num_of_ops; i++)
    {
      NPDPoint *op = list_of_ops[i].representative;
      current      = npd_SED(coord, op);

      if (min > current)
        {
          closest = i;
          min     = current;
        }
    }

  closest_point = list_of_ops[closest].representative;

  /* we want to create a new control point only when there isn't any
   * control point associated to the closest overlapping points - i.e. we
   * don't want to have two (or more) different control points manipulating
   * one overlapping points */
  if (!npd_get_control_point_at (model, closest_point))
    {
      cp.point.weight       = closest_point->weight;
      cp.overlapping_points = &list_of_ops[closest];

      npd_set_point_coordinates (&cp.point, closest_point);
      g_array_append_val (model->control_points, cp);

      if (model->hidden_model->MLS_weights)
        npd_compute_MLS_weights (model);

      return &g_array_index (model->control_points,
                             NPDControlPoint,
                             model->control_points->len - 1);
    }
  else
    return NULL;
}

/**
 * Beware, when you use this function on previously stored pointers to control
 * points it needn't to work properly, because g_array_remove_index can
 * preserve pointers but change (move) data.
 * In this situation use npd_remove_control_points instead.
 */
void
npd_remove_control_point (NPDModel        *model,
                          NPDControlPoint *control_point)
{
  gint i;
  NPDControlPoint *cp;

  for (i = 0; i < model->control_points->len; i++)
    {
      cp = &g_array_index (model->control_points, NPDControlPoint, i);

      if (cp == control_point)
        {
          npd_set_control_point_weight (cp, 1.0);
          g_array_remove_index (model->control_points, i);

          if (model->hidden_model->MLS_weights)
            npd_compute_MLS_weights (model);

          return;
        }
    }
}

gint
npd_int_sort_function_descending (gconstpointer a,
                                  gconstpointer b)
{
  return GPOINTER_TO_INT (b) - GPOINTER_TO_INT (a);
}

void
npd_remove_control_points (NPDModel *model,
                           GList    *control_points)
{
  gint i;
  NPDControlPoint *cp;
  GList *indices = NULL;

  /* first we find indices of control points we want to remove */
  while (control_points != NULL)
    {
      for (i = 0; i < model->control_points->len; i++)
        {
          cp = &g_array_index (model->control_points, NPDControlPoint, i);
          if (cp == control_points->data)
            {
              npd_set_control_point_weight (cp, 1.0);
              indices = g_list_insert_sorted (indices,
                                              GINT_TO_POINTER (i),
                                              npd_int_sort_function_descending);
            }
        }

      control_points = g_list_next (control_points);
    }

  /* indices are sorted in descending order, so we can simply iterate over them
   * and remove corresponding control points one by one */
  while (indices != NULL)
    {
      g_array_remove_index (model->control_points, GPOINTER_TO_INT (indices->data));
      indices = g_list_next (indices);
    }

  if (model->hidden_model->MLS_weights)
    npd_compute_MLS_weights (model);

  g_list_free (indices);
}

void
npd_remove_all_control_points (NPDModel *model)
{
  g_array_remove_range (model->control_points,
                        0,
                        model->control_points->len);
}

void
npd_set_control_point_weight (NPDControlPoint *cp,
                              gfloat           weight)
{
  npd_set_overlapping_points_weight(cp->overlapping_points, weight);
}

gboolean
npd_equal_coordinates (NPDPoint *p1,
                       NPDPoint *p2)
{
  return npd_equal_coordinates_epsilon(p1, p2, NPD_EPSILON);
}

gboolean
npd_equal_coordinates_epsilon (NPDPoint *p1,
                               NPDPoint *p2,
                               gfloat    epsilon)
{
  if (npd_equal_floats_epsilon (p1->x, p2->x, epsilon) &&
      npd_equal_floats_epsilon (p1->y, p2->y, epsilon))
    {
      return TRUE;
    }
  
  return FALSE;
}

NPDControlPoint*
npd_get_control_point_with_radius_at (NPDModel        *model,
                                      NPDPoint        *coord,
                                      gfloat           radius)
{
  gint i;
  for (i = 0; i < model->control_points->len; i++)
    {
      NPDControlPoint *cp = &g_array_index (model->control_points,
                                            NPDControlPoint,
                                            i);
      if (npd_equal_coordinates_epsilon (&cp->point,
                                          coord,
                                          radius))
        {
          return cp;
        }
    }

  return NULL;
}

NPDControlPoint*
npd_get_control_point_at (NPDModel *model,
                          NPDPoint *coord)
{
  return npd_get_control_point_with_radius_at (model,
                                               coord,
                                               model->control_point_radius);
}

void
npd_create_list_of_overlapping_points (NPDHiddenModel *hm)
{
  gint        i, j, num_of_bones;
  NPDBone    *bone;
  NPDPoint   *point;
  GPtrArray  *list_of_ops;
  GHashTable *coords_to_cluster;
  
  list_of_ops       = g_ptr_array_new ();
  num_of_bones      = hm->num_of_bones;
  coords_to_cluster = g_hash_table_new_full
                          (g_str_hash, g_str_equal,
                           g_free,     (GDestroyNotify) g_hash_table_destroy);

  for (i = 0; i < num_of_bones; i++)
    {
      bone = &hm->current_bones[i];

      for (j = 0; j < bone->num_of_points; j++)
        {
          point =  &bone->points[j];
          add_point_to_suitable_cluster (coords_to_cluster,
                                         point,
                                         list_of_ops);
        }
    }

  hm->list_of_overlapping_points = g_new (NPDOverlappingPoints,
                                          list_of_ops->len);
  hm->num_of_overlapping_points  = list_of_ops->len;

  for (i = 0; i < list_of_ops->len; i++)
    {
      GPtrArray *op = g_ptr_array_index (list_of_ops, i);
      hm->list_of_overlapping_points[i].points = (NPDPoint**) op->pdata;
      hm->list_of_overlapping_points[i].num_of_points = op->len;
      hm->list_of_overlapping_points[i].representative =
              hm->list_of_overlapping_points[i].points[0];
      
      for (j = 0; j < op->len; j++)
        {
          NPDPoint *p = hm->list_of_overlapping_points[i].points[j];
          p->overlapping_points = &hm->list_of_overlapping_points[i];
          p->counterpart->overlapping_points = &hm->list_of_overlapping_points[i];
        }

      g_ptr_array_free (op, FALSE); /* we want to preserve the underlying
                                       array */
    }

  /* free allocated memory */
  g_hash_table_destroy (coords_to_cluster);
  g_ptr_array_free (list_of_ops, TRUE);
}

#define NPD_FLOAT_TO_STRING(name_of_string, value)                             \
/* must be freed */                                                            \
name_of_string = g_new (gchar, 10);                                            \
g_ascii_dtostr (name_of_string, 10, value);

void
add_point_to_suitable_cluster (GHashTable *coords_to_cluster,
                               NPDPoint   *point,
                               GPtrArray  *list_of_overlapping_points)
{
  gchar      *str_coord_x, *str_coord_y;
  GHashTable *coord_y;
  GPtrArray  *op;

  NPD_FLOAT_TO_STRING (str_coord_x, point->x);
  NPD_FLOAT_TO_STRING (str_coord_y, point->y);
  
  coord_y = g_hash_table_lookup (coords_to_cluster, str_coord_x);

  if (coord_y == NULL)
    {
      /* coordinate doesn't exist */
      coord_y = g_hash_table_new_full (g_str_hash,  /* is freed during   */
                                       g_str_equal, /* destroying        */
                                       g_free,      /* coords_to_cluster */
                                       NULL);       /* hash table        */
      g_hash_table_insert (coords_to_cluster, str_coord_x, coord_y);
    }
  
  op = g_hash_table_lookup (coord_y, str_coord_y);
  if (op == NULL)
    {
      op = g_ptr_array_new ();
      g_hash_table_insert (coord_y, str_coord_y, op);
      g_ptr_array_add (list_of_overlapping_points, op);
    }
  
  g_ptr_array_add (op, point);
}

void
npd_set_overlapping_points_weight (NPDOverlappingPoints *op,
                                   gfloat                weight)
{
  gint i;
  for (i = 0; i < op->num_of_points; i++)
    {
      (*op->points[i]->weight) = weight;
    }
}

void
npd_set_point_coordinates (NPDPoint *target,
                           NPDPoint *source)
{
  target->x = source->x;
  target->y = source->y;
}

/**
 * Sets type of deformation. The function doesn't perform anything if supplied
 * deformation type doesn't differ from currently set one.
 *
 * @param model
 * @param ASAP          TRUE = ASAP deformation, FALSE = ARAP deformation
 * @param MLS_weights   use weights from Moving Least Squares deformation method
 */
void
npd_set_deformation_type (NPDModel *model,
                          gboolean ASAP,
                          gboolean MLS_weights)
{
  NPDHiddenModel *hm = model->hidden_model;

  if (hm->ASAP == ASAP && hm->MLS_weights == MLS_weights) return;

  if (MLS_weights)
    npd_compute_MLS_weights (model);
  else if (hm->MLS_weights)
    npd_reset_weights (hm);

  hm->ASAP = ASAP;
  hm->MLS_weights = MLS_weights;
}

void
npd_compute_MLS_weights (NPDModel *model)
{
  NPDHiddenModel       *hm = model->hidden_model;
  NPDControlPoint      *cp;
  NPDOverlappingPoints *op;
  NPDPoint             *cp_reference, *op_reference;
  gfloat                min, SED, MLS_weight;
  gint                  i, j;

  if (model->control_points->len == 0)
    {
      npd_reset_weights (hm);
      return;
    }

  for (i = 0; i < hm->num_of_overlapping_points; i++)
    {
      op           = &hm->list_of_overlapping_points[i];
      op_reference = op->representative->counterpart;
      min          = INFINITY;

      for (j = 0; j < model->control_points->len; j++)
        {
          cp = &g_array_index (model->control_points,
                               NPDControlPoint,
                               j);
          cp_reference = cp->overlapping_points->representative->counterpart;

          /* TODO - use geodetic distance */
          SED = npd_SED (cp_reference,
                         op_reference);
          if (SED < min) min = SED;
        }

      if (npd_equal_floats (min, 0.0)) min = 0.0000001;
      MLS_weight = 1 / pow (min, hm->MLS_weights_alpha);
      npd_set_overlapping_points_weight (op, MLS_weight);
    }
}

void
npd_reset_weights (NPDHiddenModel *hm)
{
  NPDOverlappingPoints *op;
  gint                  i;

  for (i = 0; i < hm->num_of_overlapping_points; i++)
    {
      op  = &hm->list_of_overlapping_points[i];
      npd_set_overlapping_points_weight (op, 1.0);
    }
}

void
npd_print_model (NPDModel        *model,
                 gboolean         print_control_points)
{
  gint i;
  g_printf ("NPDModel:\n");
  g_printf ("control point radius: %f\n", model->control_point_radius);
  g_printf ("control points visible: %d\n", model->control_points_visible);
  g_printf ("mesh visible: %d\n", model->mesh_visible);
  g_printf ("texture visible: %d\n", model->texture_visible);
  g_printf ("mesh square size: %d\n", model->mesh_square_size);

  npd_print_hidden_model (model->hidden_model, FALSE, FALSE);

  if (print_control_points)
    {
      g_printf ("%d control points:\n", model->control_points->len);
      for (i = 0; i < model->control_points->len; i++)
        {
          NPDControlPoint *cp = &g_array_index (model->control_points,
                                                NPDControlPoint,
                                                i);
          npd_print_point (&cp->point, TRUE);
        }
    }
}

void
npd_print_hidden_model (NPDHiddenModel *hm,
                        gboolean        print_bones,
                        gboolean        print_overlapping_points)
{
  gint i;
  g_printf ("NPDHiddenModel:\n");
  g_printf ("number of bones: %d\n", hm->num_of_bones);
  g_printf ("ASAP: %d\n", hm->ASAP);
  g_printf ("MLS weights: %d\n", hm->MLS_weights);
  g_printf ("number of overlapping points: %d\n", hm->num_of_overlapping_points);
  
  if (print_bones)
    {
      g_printf ("bones:\n");
      for (i = 0; i < hm->num_of_bones; i++)
        {
          npd_print_bone (&hm->current_bones[i]);
        }
    }
  
  if (print_overlapping_points)
    {
      g_printf ("overlapping points:\n");
      for (i = 0; i < hm->num_of_overlapping_points; i++)
        {
          npd_print_overlapping_points (&hm->list_of_overlapping_points[i]);
        }
    }
}

void
npd_print_bone (NPDBone *bone)
{
  gint i;
  g_printf ("NPDBone:\n");
  g_printf ("number of points: %d\n", bone->num_of_points);
  g_printf ("points:\n");
  for (i = 0; i < bone->num_of_points; i++)
    {
      npd_print_point (&bone->points[i], TRUE);
    }
}

void
npd_print_point (NPDPoint *point,
                 gboolean  print_details)
{
  if (print_details)
    {
      g_printf ("(NPDPoint: x: %f, y: %f, weight: %f, fixed: %d)\n",
              point->x, point->y, *point->weight, point->fixed);
    }
  else
    {
      g_printf ("(NPDPoint: x: %f, y: %f)\n",
              point->x, point->y);
    }
}

void
npd_print_overlapping_points (NPDOverlappingPoints *op)
{
  gint i;
  g_printf ("NPDOverlappingPoints:\n");
  g_printf ("number of points: %d\n", op->num_of_points);
  g_printf ("representative: ");
  npd_print_point (op->representative, TRUE);
  g_printf ("points:\n");
  for (i = 0; i < op->num_of_points; i++)
    {
      npd_print_point (op->points[i], TRUE);
    }
}
