OBJ = *.c

TARGET = libgst-jpeg-swdec.so

all: $(TARGET)

CFLAGS = -Wall -Wextra -fPIC -Os
LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Os -lm -lpthread -ljpeg

CFLAGS += \
	-I inc/ \
	-I$(STAGING_DIR)/usr/include/


$(TARGET): $(OBJ)
	$(CC) $(TARGET_CFLAGS) $(CFLAGS) -D_FILE_OFFSET_BITS=64 -Wall -I$(STAGING_DIR)/usr/include/ -L$(STAGING_DIR)/usr/lib $(LDFLAGS) $(OBJ) -shared -fPIC -o $@

.PHONY: clean

clean:
	rm -f *.o $(TARGET) $(TEST)

install:
	cp inc/*.h $(STAGING_DIR)/usr/include/
	cp $(TARGET) $(TARGET_DIR)/usr/lib/
	rm $(TARGET)


