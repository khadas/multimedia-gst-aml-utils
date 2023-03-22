OBJ = *.c

TARGET = libgst-aml-gfx2d.so

all: $(TARGET)

CFLAGS = $(GST_CFLAGS) -Wall -Wextra -fPIC -Os
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-allocators-1.0 gstreamer-video-1.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-allocators-1.0 gstreamer-video-1.0)

LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -lge2d
CFLAGS += \
	-I inc/ \
	-I$(STAGING_DIR)/usr/include/

$(info "the value of STAGING_DIR is$(STAGING_DIR)")

$(TARGET): $(OBJ)
	$(CC) $(TARGET_CFLAGS) $(CFLAGS) -D_FILE_OFFSET_BITS=64 -Wall -I$(STAGING_DIR)/usr/include/ -L$(STAGING_DIR)/usr/lib $(LDFLAGS) $(OBJ) -shared -fPIC -o $@

.PHONY: clean

clean:
	rm -f *.o $(TARGET) $(TEST)

install:
	cp inc/*.h $(STAGING_DIR)/usr/include/
	cp $(TARGET) $(TARGET_DIR)/usr/lib/

