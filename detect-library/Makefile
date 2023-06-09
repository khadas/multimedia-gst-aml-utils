OBJ = *.c *.cpp

TARGET = libdetect-library.so

all: $(TARGET)

CXXFLAGS+=-std=c++14 -Wall -Wextra -fPIC -Os
CFLAGS = -Wall -Wextra -fPIC -Os
LDFLAGS +=  -L$(STAGING_DIR)/usr/lib/ -Wall -Wextra -fPIC -Os -lstdc++ -lm -lpthread -lnnsdk

LDFLAGS +=  -L/lib/lib64

CFLAGS += \
	-I inc/ \
	-I$(STAGING_DIR)/usr/include/

$(info "the value of CFLAGS is$(CFLAGS)")
$(info "the value of LDFLAGS is$(LDFLAGS)")
$(info "the value of STAGING_DIR is$(STAGING_DIR)")
$(info "the value of TARGET_DIR is$(TARGET_DIR)")

$(TARGET): $(OBJ)
	$(CXX) $(TARGET_CFLAGS) $(CFLAGS) $(CXXFLAGS)  -D_FILE_OFFSET_BITS=64 -Wall -I$(STAGING_DIR)/usr/include/ -L$(STAGING_DIR)/usr/lib $(LDFLAGS) $(OBJ) -shared -fPIC -o $@

.PHONY: clean

clean:
	rm -f *.o $(TARGET) $(TEST)

install:
	cp inc/*.h $(STAGING_DIR)/usr/include/
	cp $(TARGET) $(TARGET_DIR)/usr/lib/
	rm $(TARGET)
	cp lib/lib64/*.so $(TARGET_DIR)/usr/lib/
	cp -rf nn_data/ $(TARGET_DIR)/etc/


