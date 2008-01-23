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
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */
#ifdef GEGL_CHANT_PROPERTIES
   /* no properties */
#else

#define GEGL_CHANT_C_FILE        "nop.c"
#define GEGL_CHANT_TYPE_FILTER
#include "gegl-chant.h"


static gboolean
process (GeglOperation       *operation,
         GeglNodeContext     *context,
         const gchar         *output_prop,
         const GeglRectangle *result)
{
  GeglBuffer *input;

  if (strcmp (output_prop, "output"))
    {
      g_warning ("requested processing of %s pad on a nop", output_prop);
      return FALSE;
    }

  input  = gegl_node_context_get_source (context, "input");
  if (!input)
    {
      g_warning ("nop received NULL input");
      return FALSE;
    }

  gegl_node_context_set_object (context, "output", G_OBJECT (input));
  return TRUE;
}

static void
operation_class_init (GeglChantClass *klass)
{
  GeglOperationClass *operation_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  operation_class->process = process;

  operation_class->name       = "nop";
  operation_class->categories = "core";
  operation_class->description = "no operation (can be used as a routing point)";
}

#endif
