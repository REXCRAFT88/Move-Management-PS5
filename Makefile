# AnyPad PS5: Bluetooth game controllers on a jailbroken PS5.
#
#   make test                 builds and runs the tests on this machine
#   make ps5                  builds the payload (needs PS5_PAYLOAD_SDK)
#   make send PS5_HOST=ip     sends it to the console's payload loader

PS5_HOST ?= ps5
PS5_PORT ?= 9021
.DEFAULT_GOAL := all

CORE   := src/util.c src/log.c src/crc32.c src/smp_crypto.c src/profiles.c src/generic.c src/host.c src/le.c
WEB    := src/web.c src/web_page.c
HOST_CFLAGS := -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -std=c11 -Wall -Wextra -Werror -O1 -g -Isrc

BUILD := build

.PHONY: all test fuzz demo ps5 send clean test_move_config

$(BUILD)/test_aim_setup: tests/test_aim_setup.cpp src/aim_setup.cpp src/aim_setup.h integrations/aim_screen_profile.hpp third_party/porpoise_aim/porpoise_aim.cpp third_party/porpoise_aim/porpoise_aim.hpp | $(BUILD)
	$(CXX) -std=c++17 -Wall -Wextra -Werror -O2 tests/test_aim_setup.cpp src/aim_setup.cpp third_party/porpoise_aim/porpoise_aim.cpp -o $@

all: test

$(BUILD)/test_hci_external: src/hci_external.c src/evstream.c src/log.c src/util.c tests/test_hci_external.c tests/mocks/libusb.h | $(BUILD)
	$(CC) $(HOST_CFLAGS) -Wno-misleading-indentation -DEXTERNAL_BT_TEST -Itests/mocks -o $@ src/hci_external.c src/evstream.c src/log.c src/util.c tests/test_hci_external.c -ldl

$(BUILD)/test_external_registration: tests/test_external_registration.c src/usb_register.c src/log.c src/util.c tests/mocks/libusb.h | $(BUILD)
	$(CC) $(HOST_CFLAGS) -Wno-misleading-indentation -Itests/mocks -o $@ tests/test_external_registration.c src/log.c src/util.c -ldl -pthread

$(BUILD):
	@mkdir -p $(BUILD)

# The web page, embedded as a byte array (kept in git, so the console build
# does not need xxd).
src/web_page.c: web/index.html
	{ echo '/* Generated from web/index.html by make. */'; \
	  echo 'const unsigned char web_page_html[] = {'; xxd -i < $<; echo '};'; \
	  echo 'const unsigned int web_page_html_len = sizeof web_page_html;'; } > $@

$(BUILD)/test_web: $(CORE) $(WEB) tests/sim.c tests/test_web.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The page against a simulated pad, to look at in a browser: make demo
demo: $(CORE) $(WEB) tests/sim.c tests/web_demo.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $(BUILD)/web_demo $^
	./$(BUILD)/web_demo

$(BUILD)/test_host: $(CORE) tests/sim.c tests/test_host.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_profiles: $(CORE) tests/test_profiles.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_crypto: src/smp_crypto.c tests/test_crypto.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_le: $(CORE) tests/sim_le.c tests/test_le.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The console source for virtual pads, against stand-ins for the console's libraries.
$(BUILD)/test_vpad: src/ps5_vpad.c src/log.c src/util.c tests/test_vpad.c | $(BUILD)
	cc -std=c11 -Wall -Wextra -Werror -O1 -g -Isrc -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -o $@ $^

# Configuration, the console hotkey and the local address.
$(BUILD)/test_config: src/config.c src/hotkey.c src/netinfo.c src/log.c src/util.c tests/test_config.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The instance lock.
$(BUILD)/test_lock: src/lock.c src/log.c src/util.c tests/test_lock.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

src/icon_data.c: assets/icon0.png
	{ echo '/* Generated from assets/icon0.png by make. */'; \
	  echo 'const unsigned char icon_png_data[] = {'; xxd -i < $<; echo '};'; \
	  echo 'const unsigned int icon_png_data_len = sizeof icon_png_data;'; } > $@

$(BUILD)/test_launcher: src/launcher.c src/icon_data.c tests/test_launcher.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# Endpoints from the chip's descriptor (checked against a real dump) and event reassembly.
$(BUILD)/test_usb: src/usb_desc.c src/evstream.c tests/test_usb.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

test: $(BUILD)/test_usb $(BUILD)/test_lock $(BUILD)/test_config $(BUILD)/test_vpad $(BUILD)/test_crypto $(BUILD)/test_profiles $(BUILD)/test_host $(BUILD)/test_le $(BUILD)/test_web $(BUILD)/test_launcher
	./$(BUILD)/test_usb
	./$(BUILD)/test_lock
	./$(BUILD)/test_config
	./$(BUILD)/test_vpad
	./$(BUILD)/test_crypto
	./$(BUILD)/test_profiles
	./$(BUILD)/test_host
	./$(BUILD)/test_le
	./$(BUILD)/test_web
	./$(BUILD)/test_launcher

# Malformed input through every parser, under the sanitizers.
SAN := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
fuzz: | $(BUILD)
	cc $(HOST_CFLAGS) $(SAN) -o $(BUILD)/test_fuzz $(CORE) tests/test_fuzz.c
	./$(BUILD)/test_fuzz

# ---- the console payload ----------------------------------------------------

ps5: src/web_page.c
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined)
endif
	$(MAKE) -f ps5.mk

send: ps5
	@echo 'Deploy one explicitly selected dist/MoveManagement-*.elf with prospero-deploy; do not send a wildcard.'

clean:
	rm -rf $(BUILD)

# Diagnostic pairing gate, independent of actual /data files.
$(BUILD)/test_diagnostic_lock: $(CORE) tests/test_diagnostic_lock.c | $(BUILD)
	cc $(HOST_CFLAGS) -DANYPAD_RAW_DIAGNOSTIC -Wl,--wrap=access -o $@ $^

$(BUILD)/test_usb_nav_seed: $(CORE) tests/sim.c tests/test_usb_nav_seed.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_usb_move_seed: $(CORE) tests/sim.c tests/test_usb_move_seed.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_porpoise_move: src/porpoise_move.c tests/test_porpoise_move.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^ -lm

$(BUILD)/test_porpoise_nav: src/porpoise_nav.c tests/test_porpoise_nav.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_move_config: src/move_config.c tests/test_move_config.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

test_move_config: $(BUILD)/test_move_config
	./$(BUILD)/test_move_config

$(BUILD)/test_studio_web: $(CORE) $(WEB) src/move_config.c tests/sim.c tests/test_studio_web.c | $(BUILD)
	cc $(HOST_CFLAGS) -DMOVE_STUDIO -o $@ $^

$(BUILD)/test_players: src/players.c tests/test_players.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_usb_registration: src/usb_register.c tests/test_usb_registration.c tests/mocks/libusb.h | $(BUILD)
	clang $(HOST_CFLAGS) -Itests/mocks -o $@ tests/test_usb_registration.c -pthread -ldl

$(BUILD)/test_four_links: tests/test_four_links.c $(filter-out src/host.c,$(CORE)) | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_eye_detector: camera/eye-multisphere-test.c camera/eye-multisphere.h | $(BUILD)
	cc $(HOST_CFLAGS) -Icamera -o $@ camera/eye-multisphere-test.c

.PHONY: camera
UVC_SOURCES := $(addprefix camera/vendor/libuvc/src/,ctrl.c ctrl-gen.c device.c diag.c frame.c init.c stream.c misc.c)
camera:
	@mkdir -p dist
	$(PS5_PAYLOAD_SDK)/bin/prospero-clang -O2 -Wall -Icamera/config -Icamera/vendor/libuvc/include -DLIBUVC_NUM_TRANSFER_BUFS=8 -DLIBUVC_PACKETS_PER_TRANSFER_MAX=8 camera/eye-optical-bridge.c camera/uvc-camera.c camera/phone-camera.c camera/ps5-usb.c $(UVC_SOURCES) -o dist/MoveManagement-Camera.elf -ldl -lpthread


$(BUILD)/test_uvc_camera: tests/test_uvc_camera.c camera/uvc-camera.c camera/rgb-normalize.h | $(BUILD)
	cc $(HOST_CFLAGS) -Wno-unused-parameter -Icamera/config -Icamera/vendor/libuvc/include -o $@ tests/test_uvc_camera.c -lm

$(BUILD)/test_eye_preview: tests/test_eye_preview.c camera/eye-preview.h camera/rgb-normalize.h | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ tests/test_eye_preview.c

$(BUILD)/test_mjpeg_stream: tests/test_mjpeg_stream.c camera/mjpeg-stream.h camera/phone-source.h | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ tests/test_mjpeg_stream.c
