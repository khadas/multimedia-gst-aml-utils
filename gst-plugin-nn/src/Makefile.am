# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstamlnn.la

# sources used to compile this plug-in
libgstamlnn_la_SOURCES = gstamlnn.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstamlnn_la_CFLAGS = $(GST_CFLAGS) \
			-Wall -Wextra -fPIC -Os \
			-I$(STAGING_DIR)/usr/include/
			$(SQLITE3_CFLAGS)

libgstamlnn_la_LIBADD = $(GST_LIBS) \
			$(SQLITE3_LIBS) \
			-lgst-aml-dma-allocator \
			-lgst-aml-gfx2d \
			-ldetect-library \
			-lnnsdk

libgstamlnn_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Wl,--as-needed -Os
libgstamlnn_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstamlnn.h
