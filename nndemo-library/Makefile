LOCAL_PATH = $(shell pwd)
TARGET = libnndemo.so
LDFLAGS += -Wl,--no-as-needed -lstdc++ -lpthread -lz -ldl -lrt -lnnsdk -shared -L$(STAGING_DIR)/usr/lib
CFLAGS += \
	-Wall -Werror -fPIC -Wextra -Os \
	-Wno-int-conversion -Wno-discarded-qualifiers \
	-Wno-incompatible-pointer-types -Wno-unused-variable \
	-Wno-sign-compare -Wno-unused-parameter

CFLAGS += \
	-I$(LOCAL_PATH)/include/common \
	-I$(LOCAL_PATH)/include/demo \
	-I$(STAGING_DIR)/usr/include/

CXXFLAGS += \
	-Wall -Werror -fPIC -Wextra -Os -std=c++14 \
	-Wextra -Wno-unused-variable -Wno-deprecated-declarations \
	-Wno-unused-function  -Wno-unused-parameter -Wno-sign-compare

CXXFLAGS += \
	-I$(LOCAL_PATH)/include/common \
	-I$(LOCAL_PATH)/include/demo \
	-I$(STAGING_DIR)/usr/include/

OUT_DIR ?= .

#C++ config
CPP_SRCS = $(wildcard $(LOCAL_PATH)/src/demo/verisilicon/*.cpp)
CPP_OBJDIR = objcpp
CPP_OBJS = $(patsubst $(LOCAL_PATH)/src/demo/verisilicon/%.cpp,$(OUT_DIR)$(CPP_OBJDIR)/%.o,$(CPP_SRCS))


#C config
C_SRCS = $(wildcard $(LOCAL_PATH)/src/demo/verisilicon/*.c)
C_OBJDIR = objc
C_OBJS = $(patsubst $(LOCAL_PATH)/src/demo/verisilicon/%.c,$(OUT_DIR)$(C_OBJDIR)/%.o,$(C_SRCS))



$(info "CXXFLAGS : $(CXXFLAGS)")
$(info "CFLAGS : $(CFLAGS)")
$(info "LDFLAGS : $(LDFLAGS)")
$(info "OUT_DIR : $(OUT_DIR)")
$(info "CPP_OBJDIR : $(CPP_OBJDIR)")
$(info "CPP_SRCS : $(CPP_SRCS)")
$(info "CPP_OBJS : $(CPP_OBJS)")
$(info "C_OBJDIR : $(C_OBJDIR)")
$(info "C_OBJS : $(C_OBJS)")
$(info "C_SRCS : $(C_SRCS)")


all: $(TARGET)

OBJS = $(CPP_OBJS) $(C_OBJS)
OBJDIRS = $(CPP_OBJDIR) $(C_OBJDIR)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(OUT_DIR)/$@ $^

CREATE_CDIR:
	@mkdir -p $(OUT_DIR)$(C_OBJDIR)

CREATE_CPPDIR:
	@mkdir -p $(OUT_DIR)$(CPP_OBJDIR)

$(OUT_DIR)$(CPP_OBJDIR)/%.o: $(LOCAL_PATH)/src/demo/verisilicon/%.cpp | CREATE_CPPDIR
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OUT_DIR)$(C_OBJDIR)/%.o: $(LOCAL_PATH)/src/demo/verisilicon/%.c | CREATE_CDIR
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: clean

clean:
	rm -f *.o $(OUT_DIR)/$(TARGET) $(OUT_DIR)/$(TEST)

install:
	install -m 644 include/common/*.h $(STAGING_DIR)/usr/include
	install -m 644 include/demo/*.h $(STAGING_DIR)/usr/include
	install -m 644 $(OUT_DIR)/$(TARGET) $(TARGET_DIR)/usr/lib


