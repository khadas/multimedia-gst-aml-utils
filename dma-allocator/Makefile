OBJ = *.c

TARGET = libdma-allocator.so

all: $(TARGET)

CFLAGS = $(GST_CFLAGS) -Wall -Wextra -fPIC -Os
CFLAGS += $(shell $(PKG_CONFIG) --cflags )
LDFLAGS += $(shell $(PKG_CONFIG) --libs -Os )

LDFLAGS += -L$(STAGING_DIR)/usr/lib/
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
	rm $(TARGET)

