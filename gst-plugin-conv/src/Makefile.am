# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstamlvconv.la

# sources used to compile this plug-in
libgstamlvconv_la_SOURCES = gstamlvconv.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstamlvconv_la_CFLAGS = $(GST_CFLAGS) \
			     -I$(STAGING_DIR)/usr/include/


libgstamlvconv_la_LIBADD = $(GST_LIBS) \
		    	     -lgst-aml-ion-allocator \
	    	             -lgst-aml-imgproc \
		    	     -lgst-aml-gfx2d \
		    	     -lge2d

libgstamlvconv_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstamlvconv_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstamlvconv.h
