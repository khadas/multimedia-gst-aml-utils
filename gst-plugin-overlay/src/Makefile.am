# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstamloverlay.la

# sources used to compile this plug-in
libgstamloverlay_la_SOURCES = gstamloverlay.c
libgstamloverlay_la_SOURCES += gstamlbaseoverlay.c
libgstamloverlay_la_SOURCES += gstamlnnoverlay.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstamloverlay_la_CFLAGS = $(GST_CFLAGS) \
						-Wall -Wextra -fPIC -Os \
			     			-I$(STAGING_DIR)/usr/include/

libgstamloverlay_la_LIBADD = $(GST_LIBS) \
 		    	     		-lgst-aml-ion-allocator \
 		    	     		-lgst-aml-gfx2d \
 		    	     		-lge2d

libgstamloverlay_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Os
libgstamloverlay_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstamlnnoverlay.h
