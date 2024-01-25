TARGET = libdetect-library.so
OUT_DIR ?= .

SRCS=$(wildcard *.cpp *.cc *.c)

all: $(TARGET)

CXXFLAGS+=-std=c++14 -Wall -Wextra -fPIC -Os
CFLAGS = -Wall -Wextra -fPIC -Os
LDFLAGS +=  -L$(STAGING_DIR)/usr/lib/ -Wall -Wextra -fPIC -Os -lstdc++ -lm -lpthread -lnnsdk -lnndemo -llog

CFLAGS += \
	-I inc/ \
	-I$(STAGING_DIR)/usr/include/

LDFLAGS += -L$(STAGING_DIR)/usr/lib/ \
			-L$(OUT_DIR)/$(OBJDIR)

$(info "TARGET_CFLAGS : $(TARGET_CFLAGS)")
$(info "CXXFLAGS : $(CXXFLAGS)")
$(info "CFLAGS : $(CFLAGS)")
$(info "LDFLAGS : $(LDFLAGS)")
$(info "OUT_DIR : $(OUT_DIR)")
$(info "STAGING_DIR : $(STAGING_DIR)")
$(info "TARGET_DIR : $(TARGET_DIR)")


$(TARGET): $(SRCS)
	@echo " [TARGET] $(TARGET)"
	@echo " [SRCS] $(SRCS)"
	$(CC) $(TARGET_CFLAGS) $(CFLAGS) -D_FILE_OFFSET_BITS=64 -Wall $(SRCS) $(LDFLAGS) -shared -fPIC -o $(OUT_DIR)/$@


.PHONY: clean

clean:
	rm -f *.o $(OUT_DIR)/$(TARGET) $(OUT_DIR)/$(TEST)

install:
	install -m 644 inc/*.h $(STAGING_DIR)/usr/include
	install -m 644 nn_data/ $(TARGET_DIR)/etc
	install -m 644 nn_input/ $(TARGET_DIR)/data
	install -m 644 $(OUT_DIR)/$(TARGET) $(TARGET_DIR)/usr/lib

