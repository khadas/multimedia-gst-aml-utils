OBJ = *.c
OUT_DIR ?= .

TARGET = libvideotranscoding.so

all: $(TARGET)

CFLAGS = $(GST_CFLAGS) -Wall -Wextra -fPIC -Os
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 gstreamer-video-1.0 glib-2.0)
CFLAGS += $(EXT_CFLAGS)
CFLAGS += \
	-I inc/ \
	-I$(STAGING_DIR)/usr/include/

LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
#LDFLAGS += -lgstreamer-1.0 -lgstbase-1.0 -lgstapp-1.0 -lgstvideo-1.0 -lgobject-2.0 -lglib-2.0
LDFLAGS += -lpthread


$(TARGET): $(OBJ)
	$(CC) $(TARGET_CFLAGS) $(CFLAGS) -D_FILE_OFFSET_BITS=64 -Wall -I$(STAGING_DIR)/usr/include/ -L$(STAGING_DIR)/usr/lib $(LDFLAGS) $(OBJ) -shared -fPIC -o $(OUT_DIR)/$@

.PHONY: clean

clean:
	rm -f *.o $(OUT_DIR)/$(TARGET)

install:
	install -m 644 inc/*.h $(STAGING_DIR)/usr/include
	install -m 644 $(OUT_DIR)/$(TARGET) $(TARGET_DIR)/usr/lib


