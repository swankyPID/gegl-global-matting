#ifdef CHANT_SELF

chant_string(window_title, "window_title", "Title to be given output window")
chant_string(icon_title, "icon_title", "Icon to be used for output window")
chant_pointer(screen, "private")
chant_int(w, 0, 1000, 0, "private")
chant_int(h, 0, 1000, 0, "private")
chant_int(width, 0, 1000, 0, "private")
chant_int(height, 0, 1000, 0, "private")

#else

#define CHANT_FILTER
#define CHANT_NAME        display
#define CHANT_DESCRIPTION "Displays the input buffer in an SDL window (restricted to one display op/process, due to SDL implementation issues, a gtk+ based replacement would be nice."
#define CHANT_SELF        "display.c"
#include "gegl-chant.h"

#include <SDL.h>
#include <signal.h>

static void
sighandler (int signal)
{
  switch (signal)
    {                           /* I want my ctrl+C back! :) */
    case SIGINT:
    case SIGTERM:
    case SIGQUIT:
      exit (0);
      break;
    }
}

static void
init_sdl (void)
{
  static int inited = 0;

  if (!inited)
    {
      inited = 1;

      signal (SIGINT, sighandler);
      signal (SIGQUIT, sighandler);

      if (SDL_Init (SDL_INIT_VIDEO) < 0)
        {
          fprintf (stderr, "Unable to init SDL: %s\n", SDL_GetError ());
          return;
        }
      atexit (SDL_Quit);
      SDL_EnableUNICODE (1);
    }
}

/*static int instances = 0;*/


static gboolean
evaluate (GeglOperation *operation,
          const gchar *output_prop)
{
  GeglOperationFilter *op_filter = GEGL_OPERATION_FILTER (operation);
  ChantInstance *self = CHANT_INSTANCE (operation);
  GeglBuffer   *source;
  GeglRect     *need    = gegl_operation_need_rect (operation);
  SDL_Surface **sdl_outwin = NULL;      //op_sym (op, "sdl_outwin");

  if(strcmp("output", output_prop))
    return FALSE;

  /*g_warning ("%s", ((Babl*)(input->format))->instance.name);*/

  g_assert (op_filter->input);

  init_sdl ();

  if (!self->screen ||
       self->w != need->w||
       self->h != need->h)
    {
      if (sdl_outwin)
        {
          if (self->screen)
            {
              SDL_FreeSurface (self->screen);
              self->screen = NULL;
            }

          self->screen = SDL_CreateRGBSurface (SDL_SWSURFACE,
                                            need->w, need->h, 32, 0xff0000,
                                            0x00ff00, 0x0000ff, 0x000000);

          *sdl_outwin = self->screen;
          if (!self->screen)
            {
              fprintf (stderr, "CreateRGBSurface failed: %s\n",
                       SDL_GetError ());
              return -1;
            }
        }
      else
        {
          self->screen = SDL_SetVideoMode (need->w, need->h, 32, SDL_SWSURFACE);
          if (!self->screen)
            {
              fprintf (stderr, "Unable to set SDL mode: %s\n",
                       SDL_GetError ());
              return -1;
            }
        }
      self->w = need->w;
      self->h = need->h;
    }

  /*
   * There seems to be a valid faster path to the SDL desired display format
   * in B'G'R'A, perhaps babl should have been able to figure this out itself?
   *
   */
  source = g_object_new (GEGL_TYPE_BUFFER,
                         "source", op_filter->input,
                         "x",      need->x,
                         "y",      need->y,
                         "width",  need->w,
                         "height", need->h,
                         NULL);
  gegl_buffer_get_fmt (source, ((SDL_Surface*)self->screen)->pixels,
       babl_format_new (babl_model ("R'G'B'A"),
                        babl_type ("u8"),
                        babl_component ("B'"),
                        babl_component ("G'"),
                        babl_component ("R'"),
                        babl_component ("A"),
                        NULL));
  g_object_unref (source);

  if (!sdl_outwin)
    {
      SDL_UpdateRect (self->screen, 0, 0, 0, 0);
      SDL_WM_SetCaption (self->window_title, self->icon_title);
    }

  self->width = need->w;
  self->height = need->h;

  op_filter->output = NULL;
  return  TRUE;
}

#endif
