include $(top_srcdir)/operations/Makefile-common.am

ops =

# No dependencies
ops += fx-fill.la
fx_fill_la_SOURCES = fx-fill.c
fx_fill_la_LIBADD = $(op_libs)

# No dependencies
ops += fx-color.la
fx_fill_la_SOURCES = fx-color.c
fx_fill_la_LIBADD = $(op_libs)

# No dependencies
ops += fx-gradient.la
fx_fill_la_SOURCES = fx-gradient.c
fx_fill_la_LIBADD = $(op_libs)

opdir = $(libdir)/gegl-@GEGL_API_VERSION@
op_LTLIBRARIES = $(ops)
