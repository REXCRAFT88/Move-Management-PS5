# The console payload, built with ps5-payload-sdk. Called from Makefile.

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

ELF   := dist/MoveManagement-Internal-beta6.12.elf
BUILD := build/ps5-move-nav

CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -Isrc -DANYPAD_RAW_DIAGNOSTIC -DMOVE_STUDIO
LDLIBS  += -lScePad -lSceUserService -ldl

SRCS := src/util.c src/log.c src/crc32.c src/smp_crypto.c src/profiles.c src/generic.c \
        src/host.c src/le.c src/porpoise_move.c src/porpoise_nav.c \
        src/web.c src/web_page.c \
        src/evstream.c src/lock.c src/netinfo.c src/usb_desc.c \
        src/hci_usb.c src/ps5_power.c src/ps5_sysinfo.c src/ps5_vpad.c src/move_config.c src/players.c src/usb_register.c src/ps5_main.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# External-only build has a distinct directory and omits the internal
# transport object entirely. No runtime fallback or implicit USB pairing.
ifeq ($(EXTERNAL_BT),1)
ELF := dist/MoveManagement-External-beta6.12.elf
BUILD := build/ps5-external-asus
CFLAGS += -DMOVE_STUDIO_EXTERNAL_BT
SRCS := $(filter-out src/hci_usb.c,$(SRCS)) src/hci_external.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
endif

ifeq ($(MANUAL_USB),1)
CFLAGS += -DMOVE_STUDIO_MANUAL_USB
BUILD := build/ps5-external-manual
ELF := dist/MoveManagement-External-Manual-beta6.12.elf
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
endif

ifeq ($(VIRTUAL_BETA),1)
CFLAGS += -DMOVE_STUDIO_VIRTUAL_BETA
BUILD := build/ps5-virtual-beta
ELF := dist/MoveManagement-Virtual-External-beta6.12.elf
ifneq ($(EXTERNAL_BT),1)
$(error VIRTUAL_BETA requires EXTERNAL_BT=1 for coexistence testing)
endif
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
endif

AIM_OBJS := $(BUILD)/aim_setup.o $(BUILD)/porpoise_aim.o
.DEFAULT_GOAL := $(ELF)

$(BUILD)/aim_setup.o: src/aim_setup.cpp src/aim_setup.h third_party/porpoise_aim/porpoise_aim.hpp
	@mkdir -p $(dir $@)
	$(CC)++ -std=c++17 -Wall -Wextra -Werror -O2 -fno-exceptions -fno-rtti -c $< -o $@

$(BUILD)/porpoise_aim.o: third_party/porpoise_aim/porpoise_aim.cpp third_party/porpoise_aim/porpoise_aim.hpp
	@mkdir -p $(dir $@)
	$(CC)++ -std=c++17 -Wall -Wextra -Werror -O2 -fno-exceptions -fno-rtti -c $< -o $@

$(ELF): $(OBJS) $(AIM_OBJS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJS): src/version.h
