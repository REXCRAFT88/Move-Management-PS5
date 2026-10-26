/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "eye-multisphere.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void set_pixel(uint8_t *raw, unsigned x, unsigned y,
                      unsigned r, unsigned g, unsigned b) {
    size_t at = (size_t)y * 1280u + (size_t)x * 2u;
    raw[at] = (uint8_t)g;
    raw[at + 1] = (uint8_t)r;
    raw[at + 640] = (uint8_t)b;
    raw[at + 641] = (uint8_t)g;
}

static void fill_rect(uint8_t *raw, unsigned x, unsigned y,
                      unsigned width, unsigned height,
                      unsigned r, unsigned g, unsigned b) {
    for (unsigned py = y; py < y + height; ++py)
        for (unsigned px = x; px < x + width; ++px)
            set_pixel(raw, px, py, r, g, b);
}

static void clear_frame(uint8_t *raw) {
    memset(raw, 0, EYE_MULTI_RAW_BYTES);
}

int main(void) {
    uint8_t *raw = (uint8_t *)calloc(EYE_MULTI_RAW_BYTES, 1);
    assert(raw != NULL);
    struct eye_color_detection found[EYE_COLOR_COUNT];

    /* A yellow and magenta blob must be detected independently. */
    clear_frame(raw);
    fill_rect(raw, 35, 40, 10, 8, 230, 220, 25);
    fill_rect(raw, 230, 170, 8, 10, 220, 30, 230);
    assert(eye_detect_colors(raw, EYE_MULTI_RAW_BYTES, found) == 1);
    assert(found[EYE_COLOR_YELLOW].valid && found[EYE_COLOR_YELLOW].area == 80);
    assert(found[EYE_COLOR_MAGENTA].valid && found[EYE_COLOR_MAGENTA].area == 80);
    assert(found[EYE_COLOR_YELLOW].x == 39.5f && found[EYE_COLOR_YELLOW].y == 43.5f);
    assert(found[EYE_COLOR_MAGENTA].x == 233.5f && found[EYE_COLOR_MAGENTA].y == 174.5f);
    assert(!found[EYE_COLOR_RED].valid && !found[EYE_COLOR_BLUE].valid);

    /* Saturated sphere core is accepted only with magenta support; plain
     * white room lights must not become a magenta controller identity. */
    clear_frame(raw);fill_rect(raw,100,90,20,20,255,255,255);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(!found[EYE_COLOR_MAGENTA].valid);
    fill_rect(raw,100,90,20,2,200,150,190);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(found[EYE_COLOR_MAGENTA].valid);
    assert(found[EYE_COLOR_MAGENTA].area==400);

    /* A motion-blurred magenta sphere may be elongated but remains colored. */
    clear_frame(raw);fill_rect(raw,100,90,40,12,200,150,190);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(found[EYE_COLOR_MAGENTA].valid);
    clear_frame(raw);fill_rect(raw,100,90,40,12,255,255,255);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(!found[EYE_COLOR_MAGENTA].valid);

    /* Small daylight-washed spheres retain colored support around bright cores. */
    clear_frame(raw);fill_rect(raw,140,150,5,5,170,222,250);fill_rect(raw,170,150,5,5,250,220,130);fill_rect(raw,200,150,5,5,170,250,180);fill_rect(raw,230,150,5,5,230,230,250);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(found[EYE_COLOR_BLUE].valid&&found[EYE_COLOR_RED].valid&&found[EYE_COLOR_GREEN].valid&&found[EYE_COLOR_MAGENTA].valid);
    assert(found[EYE_COLOR_BLUE].x<150&&found[EYE_COLOR_RED].x>170&&found[EYE_COLOR_GREEN].x>200&&found[EYE_COLOR_MAGENTA].x>230);

    clear_frame(raw);fill_rect(raw,80,100,8,8,245,100,10);fill_rect(raw,180,100,8,8,100,10,245);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(found[EYE_COLOR_ORANGE].valid&&found[EYE_COLOR_VIOLET].valid);
    assert(!found[EYE_COLOR_RED].valid&&!found[EYE_COLOR_BLUE].valid);

    /* Explicit wall-light exclusion rejects that region, not stationary spheres elsewhere. */
    clear_frame(raw);fill_rect(raw,80,100,8,8,245,10,10);fill_rect(raw,180,100,8,8,10,10,245);
    eye_exclusions[0]=(camera_exclusion){75,95,20,20};eye_exclusion_count=1;
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(!found[EYE_COLOR_RED].valid&&found[EYE_COLOR_BLUE].valid);
    eye_exclusion_count=0;assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));assert(found[EYE_COLOR_RED].valid);

    /* Empty Bayer input is valid but yields no detections. */
    clear_frame(raw);
    assert(eye_detect_colors(raw, EYE_MULTI_RAW_BYTES, found) == 1);
    for (unsigned i = 0; i < EYE_COLOR_COUNT; ++i) assert(!found[i].valid);

    /* Red and blue blobs remain separately classified. */
    fill_rect(raw, 60, 80, 9, 9, 230, 20, 15);
    fill_rect(raw, 200, 100, 9, 9, 10, 30, 235);
    assert(eye_detect_colors(raw, EYE_MULTI_RAW_BYTES, found) == 1);
    assert(found[EYE_COLOR_RED].valid && found[EYE_COLOR_RED].area == 81);
    assert(found[EYE_COLOR_BLUE].valid && found[EYE_COLOR_BLUE].area == 81);

    /* Large bright wall and malformed length are rejected. */
    clear_frame(raw);
    fill_rect(raw, 10, 10, 80, 80, 240, 10, 10);
    assert(eye_detect_colors(raw, EYE_MULTI_RAW_BYTES, found) == 1);
    assert(!found[EYE_COLOR_RED].valid);
    for (unsigned i = 0; i < EYE_COLOR_COUNT; ++i) found[i].valid = 1;
    assert(eye_detect_colors(raw, EYE_MULTI_RAW_BYTES - 1, found) == 0);
    for (unsigned i = 0; i < EYE_COLOR_COUNT; ++i) assert(!found[i].valid);

    /* Phone exposure can retain hue while reducing all channels below the
     * fixed Eye thresholds. Detection gain must not turn neutral/dark noise
     * into a controller, and must leave the preview buffer unchanged. */
    clear_frame(raw);eye_multi_rgb_source=1;
    fill_rect(raw,60,80,9,9,140,45,40);
    fill_rect(raw,120,80,9,9,40,140,45);
    fill_rect(raw,180,80,9,9,40,45,140);
    fill_rect(raw,240,80,9,9,140,60,135);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));
    assert(found[0].valid&&found[1].valid&&found[2].valid&&found[4].valid);
    assert(raw[80*1280+60*2+1]==140);
    clear_frame(raw);fill_rect(raw,100,90,10,10,180,175,175);
    fill_rect(raw,150,90,10,10,70,10,10);
    assert(eye_detect_colors(raw,EYE_MULTI_RAW_BYTES,found));
    for(unsigned i=0;i<EYE_COLOR_COUNT;i++)assert(!found[i].valid);
    eye_multi_rgb_source=0;
    free(raw);
    puts("eye-multisphere tests passed");
    return 0;
}
