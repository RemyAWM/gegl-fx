
# The setup is as follows:
#  * Each test comes with one <name>.xml GEGL XML and one reference
#    output file in 'reference' called <name>.<ext>, where <ext> is a
#    supported output file type.
#  * The test is run by processing the GEGL XML into an output file in
#    'output', named <name>.<ext>, and then compared with the file in
#    'reference'. If these files are equal, the test is a 'pass'.


SUBDIRS=data

EXTRA_DIST=$(wildcard $(srcdir)/*.xml) $(wildcard $(srcdir)/reference/*) run-compositions.py

# List of tests. The .sh suffix allows us to use $(wildcard $(srcdir)/*.xml) in
# EXTRA_DISTS
TESTS = \
  affine-with-negative.xml        \
  checkerboard.xml                \
  clones.xml                      \
  colors.xml                      \
  composite-transform.xml         \
  edge-laplace.xml                \
  edge-sobel.xml                  \
  fattal02.xml                    \
  gamma.xml                       \
  hdr-color.xml                   \
  mantiuk06.xml                   \
  pixelize.xml                    \
  reflect.xml                     \
  reflect2.xml                    \
  reinhard05.xml                  \
  rgb-params.xml                  \
  rotate.xml                      \
  simple-scale.xml                \
  softglow.xml                    \
  transform.xml                   \
  rectangles.xml

  #run-matting-global.xml.sh
  #run-matting-global-big.xml.sh

NO_OPENCL_TESTS = \
  rgbe-load.xml                 \
  rgbe-save.xml                 \
  pnm-raw-load.xml              \
  pnm-ascii-load.xml

# Conditional tests
if HAVE_JASPER
NO_OPENCL_TESTS += jp2-load.xml
endif
if HAVE_UMFPACK
TESTS += matting-levin.xml
endif

check-TESTS: $(TESTS)
	$(PYTHON) $(srcdir)/run-compositions.py \
	  --build-dir=$(top_builddir) --src-dir=$(top_srcdir) --xml-dir=$(srcdir) \
	  $(TESTS)
	$(PYTHON) $(srcdir)/run-compositions.py --without-opencl \
	  --build-dir=$(top_builddir) --src-dir=$(top_srcdir) --xml-dir=$(srcdir) \
	  $(NO_OPENCL_TESTS)

clean-local:
	rm -f output/*
