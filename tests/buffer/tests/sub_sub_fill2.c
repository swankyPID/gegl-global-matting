TEST ()
{
  GeglBuffer    *buffer, *sub, *subsub;
  GeglRectangle  subrect =    {5, 5, 10, 10};
  GeglRectangle  subsubrect = {3, 3, 4, 4};
  GeglRectangle  rect =       {0, 0, 20, 20};
  test_start ();
  buffer = gegl_buffer_new (&rect, babl_format_from_name ("Y float"));
  fill (buffer, 0.8);
  sub = gegl_buffer_create_sub_buffer (buffer, &subrect);

  fill (sub, 0.5);

  subsub = gegl_buffer_create_sub_buffer (sub, &subsubrect);
  fill (subsub, 1.0);
  print_buffer (buffer);
  print_buffer (subsub);
  gegl_buffer_destroy (sub);
  gegl_buffer_destroy (subsub);
  gegl_buffer_destroy (buffer);
  test_end ();
}
