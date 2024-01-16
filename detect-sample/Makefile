TARGET = nnsample
SRCS = main.cpp
OUT_DIR ?= .

all: $(TARGET)

CXXFLAGS+=-std=c++14 -Wall -Wextra -fPIC -Os
#CFLAGS = -Wall -Wextra -fPIC -Os

#LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -Wall -Wextra -fPIC -Os -lstdc++ -lm -ldl -lz -lrt -lpthread -lnndemo -lnnsdk -ldetect-library
LDFLAGS +=  -L$(STAGING_DIR)/usr/lib/ -Wall -Wextra -fPIC -Os -lstdc++ -lm -lpthread -ldetect-library -lnnsdk -lnndemo -laml-log


CFLAGS += \
	-I inc/ \
	-I$(STAGING_DIR)/usr/include/

# rules
$(info "CXXFLAGS : $(CXXFLAGS)")
$(info "CFLAGS : $(CFLAGS)")
$(info "LDFLAGS : $(LDFLAGS)")
$(info "OUT_DIR : $(OUT_DIR)")

$(TARGET): $(OBJ)
	$(CC) $(TARGET_CFLAGS) $(CXXFLAGS) -D_FILE_OFFSET_BITS=64 $(SRCS) $(LDFLAGS) -o $(OUT_DIR)/$@

.PHONY: clean

clean:
	rm -f *.o $(OUT_DIR)/$(TARGET)

install:
	cp $(OUT_DIR)/$(TARGET) $(TARGET_DIR)/usr/bin
