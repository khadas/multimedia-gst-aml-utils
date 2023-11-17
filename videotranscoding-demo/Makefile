TARGET =videotranscoding_demo
OBJ = videotranscoding_demo.c
OUT_DIR ?= .

all: $(TARGET)

CFLAGS = $(GST_CFLAGS) -Wall -Wextra -fPIC -Os
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 gstreamer-video-1.0 glib-2.0)
CFLAGS += \
	-I$(STAGING_DIR)/usr/include/

LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
#LDFLAGS += -lgstreamer-1.0 -lgstbase-1.0 -lgstapp-1.0 -lgstvideo-1.0 -lgobject-2.0 -lglib-2.0
LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -lvideotranscoding

# rules

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(OUT_DIR)/$@

.PHONY: clean

clean:
	rm -f *.o $(OUT_DIR)/$(TARGET)

install:
	cp $(OUT_DIR)/$(TARGET) $(TARGET_DIR)/usr/bin
