#include "gegl-unpremult.h"
#include "gegl-scanline-processor.h"
#include "gegl-image-iterator.h"
#include "gegl-utils.h"

static void class_init (GeglUnpremultClass * klass);
static void init (GeglUnpremult * self, GeglUnpremultClass * klass);

static GeglScanlineFunc get_scanline_func(GeglUnary * unary, GeglColorSpaceType space, GeglChannelSpaceType type);

static void unpremult_float (GeglFilter * filter, GeglImageIterator ** iters, gint width);

static gpointer parent_class = NULL;

GType
gegl_unpremult_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (GeglUnpremultClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (GeglUnpremult),
        0,
        (GInstanceInitFunc) init,
      };

      type = g_type_register_static (GEGL_TYPE_UNARY, 
                                     "GeglUnpremult", 
                                     &typeInfo, 
                                     0);
    }
    return type;
}

static void 
class_init (GeglUnpremultClass * klass)
{
  GeglUnaryClass *unary_class = GEGL_UNARY_CLASS(klass);

  parent_class = g_type_class_peek_parent(klass);

  unary_class->get_scanline_func = get_scanline_func;
}

static void 
init (GeglUnpremult * self, 
      GeglUnpremultClass * klass)
{
}

/* scanline_funcs[data type] */
static GeglScanlineFunc scanline_funcs[] = 
{ 
  NULL, 
  NULL, 
  unpremult_float, 
  NULL 
};

static GeglScanlineFunc
get_scanline_func(GeglUnary * unary,
                  GeglColorSpaceType space,
                  GeglChannelSpaceType type)
{
  return scanline_funcs[type];
}

static void                                                            
unpremult_float (GeglFilter * filter,              
               GeglImageIterator ** iters,        
               gint width)                       
{                                                                       
  gfloat **d = (gfloat**)gegl_image_iterator_color_channels(iters[0]);
  gfloat *da = (gfloat*)gegl_image_iterator_alpha_channel(iters[0]);
  gint d_color_chans = gegl_image_iterator_get_num_colors(iters[0]);

  gfloat **a = (gfloat**)gegl_image_iterator_color_channels(iters[1]);
  gfloat *aa = (gfloat*)gegl_image_iterator_alpha_channel(iters[1]);
  gint a_color_chans = gegl_image_iterator_get_num_colors(iters[1]);

  {
    gfloat *d0 = (d_color_chans > 0) ? d[0]: NULL;   
    gfloat *d1 = (d_color_chans > 1) ? d[1]: NULL;
    gfloat *d2 = (d_color_chans > 2) ? d[2]: NULL;

    gfloat *a0 = (a_color_chans > 0) ? a[0]: NULL;   
    gfloat *a1 = (a_color_chans > 1) ? a[1]: NULL;
    gfloat *a2 = (a_color_chans > 2) ? a[2]: NULL;

    switch(d_color_chans)
      {
        case 3: 
        while(width--)                                                        
          {                                                                   
            if(GEGL_FLOAT_EQUAL(*aa, 1.0))
              {
                *d0++ = *a0++;
                *d1++ = *a1++;
                *d2++ = *a2++;
                *da++ = *aa++;
              }
            else if(GEGL_FLOAT_EQUAL(*aa, 0.0))
              {
                *d0++ = 0.0; a0++;
                *d1++ = 0.0; a1++;
                *d2++ = 0.0; a2++;
                *da++ = *aa++;
              }
            else 
              {
                *d0++ = *a0++ / *aa;
                *d1++ = *a1++ / *aa;
                *d2++ = *a2++ / *aa;
                *da++ = *aa++;
              }
          }
          break;
        case 2: 
        while(width--)                                                        
          {                                                                   
            if(GEGL_FLOAT_EQUAL(*aa, 1.0))
              {
                *d0++ = *a0++;
                *d1++ = *a1++;
                *da++ = *aa++;
              }
            else if(GEGL_FLOAT_EQUAL(*aa, 0.0))
              {
                *d0++ = 0.0; a0++;
                *d1++ = 0.0; a1++;
                *da++ = *aa++;
              }
            else 
              {
                *d0++ = *a0++ / *aa;
                *d1++ = *a1++ / *aa;
                *da++ = *aa++;
              }
          }
          break;
        case 1: 
        while(width--)                                                        
          {                                                                   
            if(GEGL_FLOAT_EQUAL(*aa, 1.0))
              {
                *d0++ = *a0++;
                *da++ = *aa++;
              }
            else if(GEGL_FLOAT_EQUAL(*aa, 0.0))
              {
                *d0++ = 0.0; a0++;
                *da++ = *aa++;
              }
            else 
              {
                *d0++ = *a0++ / *aa;
                *da++ = *aa++;
              }
          }
          break;
      }
  }

  g_free(d);
  g_free(a);
}                                                                       
