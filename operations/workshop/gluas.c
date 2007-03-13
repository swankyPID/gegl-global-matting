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
 * Copyright 2004, 2006 Øyvind Kolås <pippin@gimp.org>
 */
#if GEGL_CHANT_PROPERTIES

#define THRESHOLD_SCRIPT \
"level = user_value/2\n"\
"for y=bound_y0, bound_y1 do\n"\
"  for x=bound_x0, bound_x1 do\n"\
"    v = get_value (x,y)\n"\
"    if v>level then\n"\
"      v=1.0\n"\
"    else\n"\
"      v=0.0\n"\
"    end\n"\
"    set_value (x,y,v)\n"\
"  end\n"\
"  progress (y/height)\n"\
"end"

gegl_chant_multiline (script, THRESHOLD_SCRIPT, "The lua script containing the implementation of this operation.")
gegl_chant_path (file, "", "a stored lua script on disk implementing an operation.")
gegl_chant_double (user_value, -1000.0, 1000.0, 1.0, "(appears in the global variable 'user_value' in lua.")

#else

#define GEGL_CHANT_COMPOSER
#define GEGL_CHANT_NAME            gluas
#define GEGL_CHANT_DESCRIPTION     "A general purpose filter/composer implementation proxy for the lua programming language."
#define GEGL_CHANT_SELF            "gluas.c"
#define GEGL_CHANT_CATEGORIES      "script"
#define GEGL_CHANT_INIT
#define GEGL_CHANT_CLASS_INIT
#include "gegl-chant.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct Priv
{
  gint              bpp;
  GeglBuffer       *in_drawable;
  GeglBuffer       *out_drawable;
  Babl             *rgba_float;

  gint              bx1, by1;
  gint              bx2, by2;    /* mask bounds */

  gint              width;
  gint              height;
  
  lua_State        *L;
}
Priv;

static void init (GeglChantOperation *self)
{
}

static void
drawable_lua_process (GeglChantOperation *self,
                      GeglBuffer  *drawable,
                      GeglBuffer  *result,
                      GeglRectangle *roi,
                      const gchar *file,
                      const gchar *buffer,
                      gdouble      user_value);

static gboolean
process (GeglOperation *operation,
         gpointer       context_id)
{
  GeglChantOperation  *self;
  GeglBuffer          *input;
  GeglBuffer          *output;

  self  = GEGL_CHANT_OPERATION (operation);
  input = GEGL_BUFFER (gegl_operation_get_data (operation, context_id, "input"));

    {
      GeglRectangle   *result = gegl_operation_result_rect (operation, context_id);

      if (result->width ==0 ||
          result->height==0)
        {
          output = g_object_ref (input);
        }
      else
        {
          output = g_object_new (GEGL_TYPE_BUFFER,
                                 "format", babl_format ("RGBA float"),
                                 "x",      input->x,
                                 "y",      input->y,
                                 "width",  input->width,
                                 "height", input->height,
                                 NULL);

          if (self->file && g_file_test (self->file, G_FILE_TEST_IS_REGULAR))
            {
              drawable_lua_process (self, input, output, result, self->file, NULL, self->user_value);
            }
          else
            {
              drawable_lua_process (self, input, output, result, NULL, self->script, self->user_value);
            }
        }
        gegl_operation_set_data (operation, context_id, "output", G_OBJECT (output));
    }
  return TRUE;
}

static GeglRectangle
get_defined_region (GeglOperation *operation)
{
  GeglRectangle  result = {0,0,0,0};
  GeglRectangle *in_rect = gegl_operation_source_get_defined_region (operation,
                                                                     "input");
  if (!in_rect)
    return result;

  result = *in_rect;
  return result;
}

static void class_init (GeglOperationClass *operation_class)
{
  operation_class->get_defined_region  = get_defined_region;
}


#define TILE_CACHE_SIZE   16
#define SCALE_WIDTH      125
#define ENTRY_WIDTH       50

static int l_set_rgba  (lua_State * lua);
static int l_get_rgba  (lua_State * lua);
static int l_set_rgb   (lua_State * lua);
static int l_get_rgb   (lua_State * lua);
static int l_set_hsl   (lua_State * lua);
static int l_get_hsl   (lua_State * lua);
static int l_set_hsv   (lua_State * lua);
static int l_get_hsv   (lua_State * lua);
static int l_set_value (lua_State * lua);
static int l_get_value (lua_State * lua);
static int l_set_alpha (lua_State * lua);
static int l_get_alpha (lua_State * lua);
static int l_set_lab   (lua_State * lua);
static int l_get_lab   (lua_State * lua);
static int l_in_width  (lua_State * lua);
static int l_in_height (lua_State * lua);

static int l_progress  (lua_State * lua);
static int l_flush     (lua_State * lua);
static int l_print     (lua_State * lua);

static const luaL_reg gluas_functions[] =
{
    {"set_rgba",    l_set_rgba},
    {"get_rgba",    l_get_rgba},
    {"set_rgb",     l_set_rgb},
    {"get_rgb",     l_get_rgb},
    {"set_hsl",     l_set_hsl},
    {"get_hsl",     l_get_hsl},
    {"set_hsv",     l_set_hsv},
    {"get_hsv",     l_get_hsv},
    {"set_lab",     l_set_lab},
    {"get_lab",     l_get_lab},
    {"set_value",   l_set_value},
    {"get_value",   l_get_value},
    {"set_alpha",   l_set_alpha},
    {"get_alpha",   l_get_alpha},
    {"in_width",    l_in_width},
    {"in_height",   l_in_height},
    {"progress",    l_progress},
    {"flush",       l_flush},
    {"print",       l_print},
    {NULL,          NULL}
};
static void
register_functions (lua_State      *L,
                    const luaL_reg *l)
{
  for (;l->name; l++)
    lua_register (L, l->name, l->func);
}

static void
drawable_lua_process (GeglChantOperation *self,
                      GeglBuffer    *drawable,
                      GeglBuffer    *result,
                      GeglRectangle *roi,
                      const gchar   *file,
                      const gchar   *buffer,
                      gdouble        user_value)
{
    /*GimpRGB    background;*/

    GeglRectangle *in_rect = gegl_operation_source_get_defined_region (GEGL_OPERATION (self),
                                                                       "input");

    lua_State *L;
    Priv p;

    /*cpercep_init_conversions ();*/
    /*gimp_progress_init ("gluas");*/

    /*  set the tile cache size  */
    /*gimp_tile_cache_ntiles (TILE_CACHE_SIZE);*/

    L = lua_open ();
    lua_baselibopen (L);
    lua_iolibopen (L);
    lua_strlibopen (L);
    lua_mathlibopen (L);
    lua_tablibopen (L);

    register_functions (L, gluas_functions);

    p.rgba_float = babl_format ("RGBA float");
    p.L = L;
    p.width = in_rect->width;/*gimp_drawable_width (drawable->drawable_id);*/
    p.height = in_rect->height;/*gimp_drawable_height (drawable->drawable_id);*/

    p.bx1 = roi->x;
    p.by1 = roi->y;
    p.bx2 = roi->x + roi->width;
    p.by2 = roi->y + roi->height;

    lua_pushnumber (L, (double) user_value);
    lua_setglobal (L, "user_value");
    lua_pushnumber (L, (double) p.width);
    lua_setglobal (L, "width");
    lua_pushnumber (L, (double) p.height);
    lua_setglobal (L, "height");

    lua_pushstring (L, "priv");
    lua_pushlightuserdata (L, &p);
    lua_settable (L, LUA_REGISTRYINDEX);

    p.in_drawable = drawable;
    p.out_drawable = result;

#if 0 
    p.bpp = gimp_drawable_bpp (drawable->drawable_id);
    p.pft = gimp_pixel_fetcher_new (drawable, FALSE);

    gimp_pixel_fetcher_set_edge_mode (p.pft, GIMP_PIXEL_FETCHER_EDGE_SMEAR);
    gimp_pixel_fetcher_set_bg_color (p.pft, &background); /* can be removed
                                                             as long as the
                                                             mode is smear.. */
    gimp_pixel_rgn_init (&(p.dpr), drawable, 0, 0, p.width, p.height, TRUE, TRUE);
    gimp_pixel_rgn_init (&(p.pr), drawable, 0, 0, p.width, p.height, FALSE, FALSE);

    gimp_drawable_mask_bounds (drawable->drawable_id, &(p.bx1), &(p.by1),
                               &(p.bx2), &(p.by2));


    {
      gpointer pr;
      for (pr=gimp_pixel_rgns_register (2, &(p.pr), &(p.dpr));
           pr !=NULL;
           pr = gimp_pixel_rgns_process (pr)
          )
        {
          memcpy (p.dpr.data, p.pr.data, p.dpr.rowstride *p.dpr.h);
        }
    }
#endif
    lua_pushnumber (L, (double) p.bx1);
    lua_setglobal (L, "bound_x0");
    lua_pushnumber (L, (double) p.bx2);
    lua_setglobal (L, "bound_x1");
    lua_pushnumber (L, (double) p.by1);
    lua_setglobal (L, "bound_y0");
    lua_pushnumber (L, (double) p.by2);
    lua_setglobal (L, "bound_y1");

        {
      gint status = 0;

      lua_dostring (L, "os.setlocale ('C', 'numeric')");

      if (file && file[0]!='\0')
        status = luaL_loadfile (L, file);
      else if (buffer)
        status = luaL_loadbuffer (L, buffer, strlen (buffer), "buffer");

      if (status == 0)
        status = lua_pcall (L, 0, LUA_MULTRET, 0);

      gegl_buffer_flush (p.out_drawable);
#if 0
      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_update (drawable->drawable_id, p.bx1, p.by1,
                            p.bx2 - p.bx1, p.by2 - p.by1);
#endif

      if (status != 0)
        g_warning ("lua error: %s", lua_tostring (L, -1));
      /*else
        g_warning ("gluas error");*/
    }

#if 0
  gimp_pixel_fetcher_destroy (p.pft);
#endif
}

#if 0
void
drawable_lua_do_file (GeglBuffer    *drawable,
                      GeglBuffer    *result,
                      GeglRectangle *roi,
                      const gchar   *file,
                      gdouble        user_value)
{
  drawable_lua_process (drawable, result, roi, file, NULL, user_value);
}

void
drawable_lua_do_buffer (GeglBuffer    *drawable,
                        GeglBuffer    *result,
                        GeglRectangle *roi,
                        const gchar   *buffer,
                        gdouble        user_value)
{
  drawable_lua_process (drawable, result, roi, NULL, buffer, user_value);
}
#endif

static void inline
get_rgba_pixel (void       *data,
                int         img_no,
                int         x,
                int         y,
                lua_Number  pixel[4])
{
  Priv *p;
  gfloat buf[4];
  
  p = data;
#if 0
  if (0 || x < 0 || y < 0 || (int) x >= p->width || (int) y >= p->height)
    {
      int edge_duplicate = 0;
      lua_getglobal(p->L, "edge_duplicate");
      edge_duplicate = lua_toboolean(p->L, -1);
      lua_pop(p->L, 1);

      if (edge_duplicate)
        {
          if (x < 0)
              x = 0;
          if (y < 0)
              y = 0;
          if (x >= p->width)
              x = p->width - 1;
          if (y >= p->height)
              y = p->height - 1;
        }
      else
        {
          int i;
          for (i = 0; i < 4; i++)
            pixel[i] = 0.0;
          return;
        }
    }
#endif

  gegl_buffer_sample (p->in_drawable, x, y, 1.0, buf,
                      p->rgba_float,
                      GEGL_INTERPOLATION_NEAREST);
  {
    int i;
    for (i = 0; i < 4; i++)
      pixel[i] = buf[i];
  }
}

static void inline
set_rgba_pixel (void       *data,
                int         x,
                int         y,
                lua_Number  pixel[4])
{
  Priv   *p;
  GeglRectangle roi = {x,y,1,1};
  gint    i;
  gfloat  buf[4];

  p = data;

  /*FIXME: */
#if 0
  if (x < p->bx1 || y < p->by1 || x > p->bx2 || y > p->by2)
      return;     /* outside selection, ignore */
  if (x < 0 || y < 0 || x >= p->width || y >= p->height)
      return;    /* out of drawable bounds */
#endif
  for (i = 0; i < 4; i++)
    {
      buf[i]=pixel[i];
    }

 gegl_buffer_set (p->out_drawable, &roi, p->rgba_float, buf);
}

static int
l_in_width (lua_State * lua)
{
  Priv *p;

  lua_pushstring(lua, "priv");
  lua_gettable(lua, LUA_REGISTRYINDEX);
  p = lua_touserdata(lua, -1);
  lua_pop(lua, 1);
  lua_pushnumber(lua, (double)p->width);
  return 1;
}

static int
l_in_height(lua_State * lua)
{
  Priv *p;

  lua_pushstring(lua, "priv");
  lua_gettable(lua, LUA_REGISTRYINDEX);
  p = lua_touserdata(lua, -1);
  lua_pop(lua, 1);
  lua_pushnumber(lua, (double)p->height);

  return 1;
}

static int
l_flush (lua_State * lua)
{
  Priv *p;

  lua_pushstring(lua, "priv");
  lua_gettable(lua, LUA_REGISTRYINDEX);
  p = lua_touserdata(lua, -1);

  gegl_buffer_flush (p->out_drawable);
#if 0
  gimp_drawable_flush (p->drawable);
  gimp_drawable_merge_shadow (p->drawable->drawable_id, FALSE);
#endif

  return 0;
}

static int
l_progress(lua_State * lua)
{
  lua_Number percent;

  if (!lua_gettop(lua))
    return 0;
  percent = lua_tonumber(lua, -1);

#if 0
  g_warning ("progress,.. %f", percent);
  gimp_progress_update((double) percent);
#endif
  return 0;
}

static int
l_print (lua_State * lua)
{
  if (!lua_gettop(lua))
    return 0;
  g_print ("%s\n", lua_tostring(lua, -1));

  return 0;
}


static int l_set_rgba (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 6)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_rgba (x, y, r, g, b, a)\n");
        lua_error (lua);
        return 0;
      }

    x        = lua_tonumber (lua, -6);
    y        = lua_tonumber (lua, -5);
    pixel[0] = lua_tonumber (lua, -4);
    pixel[1] = lua_tonumber (lua, -3);
    pixel[2] = lua_tonumber (lua, -2);
    pixel[3] = lua_tonumber (lua, -1);

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

static int l_get_rgba (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number pixel[4];
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgba (x, y)\n");
      lua_error (lua);
      break;
  }

  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);
  
  lua_pushnumber (lua, pixel[0]);
  lua_pushnumber (lua, pixel[1]);
  lua_pushnumber (lua, pixel[2]);
  lua_pushnumber (lua, pixel[3]);

  return 4;
}

static int l_set_rgb (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_rgb (x, y, r, g, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);

    pixel[0] = lua_tonumber (lua, -3);
    pixel[1] = lua_tonumber (lua, -2);
    pixel[2] = lua_tonumber (lua, -1);
    pixel[3] = 1.0;
    
    set_rgba_pixel (p, x, y, pixel);

    return 0;
}

static int l_get_rgb (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number pixel[4];
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb (x, y, [, image_no])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);

  lua_pushnumber (lua, pixel[0]);
  lua_pushnumber (lua, pixel[1]);
  lua_pushnumber (lua, pixel[2]);

  return 3;
}

static int l_set_value (lua_State * lua)
{
    Priv *p;
    lua_Number x, y, v;
    lua_Number pixel[4];

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 3)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_value (x, y, value)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -3);
    y = lua_tonumber (lua, -2);
    v = lua_tonumber (lua, -1);

    pixel[0] = pixel[1] = pixel[2] = v;
    pixel[3] = 1.0;

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

int l_get_value (lua_State * lua)
{
  Priv *p;
  lua_Number pixel[4];
  lua_Number x,y;
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_value (x, y [, image_no])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);

  lua_pushnumber (lua, (pixel[0]+pixel[1]+pixel[2]) * (1.0/3));

  return 1;
}

static int l_set_alpha (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, a;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 3)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_alpha (x, y, a)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -3);
    y = lua_tonumber (lua, -2);
    a = lua_tonumber (lua, -1);

    get_rgba_pixel (p, 0, x, y, pixel);
    pixel[3] = a;
    set_rgba_pixel (p, x, y, pixel);

    return 0;
}

static int l_get_alpha (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number pixel[4];
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_alpha (x, y [,image])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);
  lua_pushnumber (lua, pixel[3]);

  return 1;
}

static int l_set_lab (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, l, a, b;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_lab (x, y, l, a, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);
    l = lua_tonumber (lua, -3);
    a = lua_tonumber (lua, -2);
    b = lua_tonumber (lua, -1);

    /* pixel assumed to be of type double */
    
    get_rgba_pixel (p, 0, x, y, pixel);
#if 0
    cpercep_space_to_rgb (l, a, b, &(pixel[0]), &(pixel[1]), &(pixel[2]));
      {
      int i;
      for (i=0;i<3;i++)
        pixel[i] *= (1.0/255.0);
      }
#endif
    set_rgba_pixel (p, x, y, pixel);

    return 0;
}

static int l_get_lab (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number img_no = 0;

  lua_Number pixel[4];
  /*double lab_pixel[3];*/

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb (x, y, [, image_no])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);

#if 0
  cpercep_rgb_to_space (pixel[0] * 255.0,
                        pixel[1] * 255.0,
                        pixel[2] * 255.0,

                        &(lab_pixel[0]),
                        &(lab_pixel[1]),
                        &(lab_pixel[2]));

  lua_pushnumber (lua, lab_pixel[0]);
  lua_pushnumber (lua, lab_pixel[1]);
  lua_pushnumber (lua, lab_pixel[2]);
#endif

  return 3;
}

static int l_set_hsl (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, h, s, l;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_lab (x, y, l, a, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);
    h = lua_tonumber (lua, -3);
    s = lua_tonumber (lua, -2);
    l = lua_tonumber (lua, -1);

    get_rgba_pixel (p, 0, x, y, pixel);
   
#if 0 
    {
      GimpRGB rgb;
      GimpHSL hsl;

      hsl.h = h;
      hsl.s = s;
      hsl.l = l;
     
      gimp_hsl_to_rgb (&hsl, &rgb); 

      pixel[0]=rgb.r;
      pixel[1]=rgb.g;
      pixel[2]=rgb.b;
    }
#endif

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

static int l_get_hsl (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number img_no = 0;

  lua_Number pixel[4];

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb ([image_no,] x, y)\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

    get_rgba_pixel (p, img_no, x, y, pixel);

#if 0
    {
      GimpRGB rgb;
      GimpHSL hsl;

      rgb.r = pixel[0];
      rgb.g = pixel[1];
      rgb.b = pixel[2];

      gimp_rgb_to_hsl (&rgb, &hsl); 

      lua_pushnumber (lua, hsl.h );
      lua_pushnumber (lua, hsl.s );
      lua_pushnumber (lua, hsl.l );
    }
#endif

  return 3;
}

static int l_set_hsv (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, h, s, v;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_lab (x, y, l, a, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);
    h = lua_tonumber (lua, -3);
    s = lua_tonumber (lua, -2);
    v = lua_tonumber (lua, -1);

    get_rgba_pixel (p, 0, x, y, pixel);
   
#if 0 
    {
      GimpRGB rgb;
      GimpHSV hsv;

      hsv.h = h;
      hsv.s = s;
      hsv.v = v;
     
      gimp_hsv_to_rgb (&hsv, &rgb); 

      pixel[0]=rgb.r;
      pixel[1]=rgb.g;
      pixel[2]=rgb.b;
    }
#endif

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

static int l_get_hsv (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number img_no = 0;

  lua_Number pixel[4];

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb ([image_no,] x, y)\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

    get_rgba_pixel (p, img_no, x, y, pixel);

#if 0
    {
      GimpRGB rgb;
      GimpHSV hsv;

      rgb.r = pixel[0];
      rgb.g = pixel[1];
      rgb.b = pixel[2];

      gimp_rgb_to_hsv (&rgb, &hsv); 

      lua_pushnumber (lua, hsv.h );
      lua_pushnumber (lua, hsv.s );
      lua_pushnumber (lua, hsv.v );
    }
#endif
  return 3;
}

#endif
