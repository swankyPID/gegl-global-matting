/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */
#if GEGL_CHANT_PROPERTIES

gegl_chant_double(radius1, 0.0, 100.0, 1.0, "Radius")
gegl_chant_double(radius2, 0.0, 100.0, 2.0, "Radius")

#else

#define GEGL_CHANT_META
#define GEGL_CHANT_NAME            dog
#define GEGL_CHANT_DESCRIPTION     "does a difference of gaussians edge detection"
#define GEGL_CHANT_SELF            "dog.c"
#define GEGL_CHANT_CATEGORIES      "meta:enhance"
#define GEGL_CHANT_CLASS_INIT
#include "gegl-chant.h"

typedef struct _Priv Priv;
struct _Priv
{
  GeglNode *self;
  GeglNode *input;
  GeglNode *output;

  GeglNode *multiply;
  GeglNode *subtract;
  GeglNode *blur1;
  GeglNode *blur2;
};

static void attach (GeglOperation *operation)
{
  GeglChantOperation *self;
  Priv          *priv;

  self       = GEGL_CHANT_OPERATION (operation);
  priv       = g_malloc0 (sizeof (Priv));
  self->priv = (void*) priv;


  if (!priv->blur1)
    {
      GeglNode      *gegl;
      gegl = operation->node;

      priv->input    = gegl_graph_input (gegl, "input");
      priv->output   = gegl_graph_output (gegl, "output");

      priv->subtract = gegl_graph_new_node (gegl,
                                            "operation", "subtract",
                                            NULL);

      priv->blur1    = gegl_graph_new_node (gegl,
                                            "operation", "gaussian-blur",
                                            NULL);

      priv->blur2    = gegl_graph_new_node (gegl,
                                            "operation", "gaussian-blur",
                                            NULL);

      gegl_node_link_many (priv->input, priv->blur1, priv->subtract, priv->output, NULL);
      gegl_node_link (priv->input, priv->blur2);

      gegl_node_connect_from (priv->subtract, "aux",   priv->blur2,     "output");

      gegl_operation_meta_redirect (operation, "radius1", priv->blur1, "radius-x");
      gegl_operation_meta_redirect (operation, "radius1", priv->blur1, "radius-y");
      gegl_operation_meta_redirect (operation, "radius2", priv->blur2, "radius-x");
      gegl_operation_meta_redirect (operation, "radius2", priv->blur2, "radius-y");
    }
}

static void class_init (GeglOperationClass *klass)
{
  klass->attach = attach;
}

#endif
