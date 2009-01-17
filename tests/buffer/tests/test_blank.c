TEST ()
{
  GeglBuffer    *buffer;
  GeglRectangle  rect = {0, 0, 20, 20};

  test_start ();

  buffer = gegl_buffer_new (&rect, babl_format_from_name ("Y float"));
  print_buffer (buffer);
  gegl_buffer_destroy (buffer);

  test_end ();
}
