# Makefile for server + client (Cross-compiled for aarch64 / Raspberry Pi)

# 크로스 컴파일러
CC = aarch64-linux-gnu-gcc
GCC = gcc
CFLAGS = -fPIC -Iinclude
LDFLAGS = -lwiringPi -lpthread -ldl -rdynamic  # dlopen 추가
TARGET_CLIENT = client
TARGET_DAEMON = server

SRC_DIR = src
BUILD_DIR = build
LIBS = led_ctl buz_ctl light_ctl seg_ctl
LIB_SO = $(patsubst %, $(BUILD_DIR)/lib%.so, $(LIBS))

RASPI_USER = veda
RASPI_HOST = 192.168.0.83
RASPI_PATH = ~/pjt

.PHONY: all clean deploy

all: $(BUILD_DIR) $(LIB_SO) $(BUILD_DIR)/$(TARGET_CLIENT) $(BUILD_DIR)/$(TARGET_DAEMON)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 공유 라이브러리 빌드 (서버와 독립적)
$(BUILD_DIR)/lib%.so: $(SRC_DIR)/%.c include/%.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $(BUILD_DIR)/$*.o
	$(CC) -shared -o $@ $(BUILD_DIR)/$*.o $(LDFLAGS)

# 클라이언트 (기존과 동일)
$(BUILD_DIR)/$(TARGET_CLIENT): $(SRC_DIR)/$(TARGET_CLIENT).c
	$(GCC) -o $@ $<

# 서버 (동적 로딩 전용)
$(BUILD_DIR)/$(TARGET_DAEMON): $(SRC_DIR)/$(TARGET_DAEMON).c
	$(CC) -Iinclude -L$(BUILD_DIR) -Wl,-rpath,'\$$ORIGIN' -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

deploy: all
	scp $(BUILD_DIR)/$(TARGET_DAEMON) $(RASPI_USER)@$(RASPI_HOST):$(RASPI_PATH)/
	scp $(BUILD_DIR)/lib*.so $(RASPI_USER)@$(RASPI_HOST):$(RASPI_PATH)/
