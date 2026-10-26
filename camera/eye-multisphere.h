/* SPDX-License-Identifier: GPL-3.0-or-later
 * Multi-color image-space blob detector for 2x2-reduced 640x480 GRBG Bayer.
 * Reports only 2D image coordinates and apparent blob size; it does not infer
 * orientation, depth, or 3D position. Independent of eye-sphere.h.
 */
#ifndef EYE_MULTISPHERE_H
#define EYE_MULTISPHERE_H

#include <stddef.h>
#include "exclusions.h"
static camera_exclusion eye_exclusions[CAMERA_EXCLUSION_MAX];
static unsigned eye_exclusion_count;
#include <stdint.h>
#include <string.h>

#define EYE_MULTI_RAW_BYTES (640u * 480u)
#define EYE_MULTI_WIDTH 320u
#define EYE_MULTI_HEIGHT 240u
#define EYE_MULTI_PIXELS (EYE_MULTI_WIDTH * EYE_MULTI_HEIGHT)
#define EYE_COLOR_COUNT 8u

enum eye_color {
    EYE_COLOR_BLUE = 0,
    EYE_COLOR_RED,
    EYE_COLOR_GREEN,
    EYE_COLOR_YELLOW,
    EYE_COLOR_MAGENTA,
    EYE_COLOR_CYAN,
    EYE_COLOR_ORANGE,
    EYE_COLOR_VIOLET
};

struct eye_color_detection {
    int valid;
    unsigned area;
    float x;
    float y;
    float width;
    float height;
};

static uint8_t eye_multi_mask[EYE_MULTI_PIXELS];
static uint32_t eye_multi_queue[EYE_MULTI_PIXELS];
static int eye_multi_rgb_source; /* phone/UVC exposure differs from fixed Eye exposure */
static int eye_multi_require_chroma;
static enum eye_color eye_multi_color;
static const uint8_t *eye_multi_raw;

static inline int eye_multi_matches(enum eye_color color,
                                    unsigned r, unsigned g, unsigned b) {
    switch (color) {
    case EYE_COLOR_ORANGE: return r>=180 && r>g*14/10 && g>=70 && g>b*15/10;
    case EYE_COLOR_VIOLET: return b>=180 && b>r*14/10 && r>=70 && r>g*13/10;
    case EYE_COLOR_BLUE:
        return b >= 110 && b * 10 >= r * 13 && b * 10 >= g * 11;
    case EYE_COLOR_RED:
        return r >= 110 && r * 10 >= b * 13 && r * 10 >= g * 11;
    case EYE_COLOR_GREEN:
        return g >= 110 && g * 10 >= r * 11 && g * 10 >= b * 11;
    case EYE_COLOR_YELLOW:
        return r >= 100 && g >= 100 && b * 10 <= r * 6 &&
               b * 10 <= g * 6 && r * 10 <= g * 16 && g * 10 <= r * 16;
    case EYE_COLOR_MAGENTA:
        return r >= 100 && b >= 100 && g * 10 <= r * 6 &&
               g * 10 <= b * 6 && r * 10 <= b * 16 && b * 10 <= r * 16;
    case EYE_COLOR_CYAN:
        return g >= 100 && b >= 100 && r * 10 <= g * 6 &&
               r * 10 <= b * 6 && g * 10 <= b * 16 && b * 10 <= g * 16;
    default:
        return 0;
    }
}

static inline void eye_multi_build_mask(const uint8_t *raw, enum eye_color color) {
    eye_multi_require_chroma=color==EYE_COLOR_MAGENTA;eye_multi_color=color;eye_multi_raw=raw;
    for (unsigned y = 0; y < EYE_MULTI_HEIGHT; ++y) {
        for (unsigned x = 0; x < EYE_MULTI_WIDTH; ++x) {
            size_t at = (size_t)y * 1280u + (size_t)x * 2u;
            unsigned g_top = raw[at];
            unsigned r = raw[at + 1u];
            unsigned b = raw[at + 640u];
            unsigned g_bottom = raw[at + 641u];
            unsigned g = (g_top + g_bottom) / 2u;
            int core=r>=220&&g>=220&&b>=220;
            /* Detection-only saturation/value normalization. Keep the preview
             * untouched, reject dark/noisy or near-neutral pixels before gain.
             * Subtract a common pedestal to boost chroma without changing hue. */
            if(eye_multi_rgb_source){
                unsigned hi=r>g?r:g;hi=hi>b?hi:b;
                unsigned lo=r<g?r:g;lo=lo<b?lo:b;
                if(hi>=100 && hi-lo>=20 && (hi-lo)*100>=hi*8){
                    unsigned pedestal=lo/2,scale=hi-pedestal;
                    r=(r-pedestal)*245/scale;
                    g=(g-pedestal)*245/scale;
                    b=(b-pedestal)*245/scale;
                }else{r=g=b=0;}
            }
            int chroma=eye_multi_matches(color,r,g,b);
            if(color==EYE_COLOR_BLUE)chroma=b>=190&&b*100>=r*115&&b*10>=g*9;
            if(color==EYE_COLOR_RED)chroma=r>=220&&r*100>=b*125&&r*100>=g*107;
            if(color==EYE_COLOR_GREEN)chroma=g>=210&&g*100>=r*108&&g*100>=b*108;
            if(color==EYE_COLOR_MAGENTA)chroma=r>=170&&b>=170&&r*10>=g*9&&b*10>=g*9&&r*10<=b*13&&b*10<=r*13;
            unsigned maximum=r>g?r:g;maximum=maximum>b?maximum:b;unsigned minimum=r<g?r:g;minimum=minimum<b?minimum:b;
            chroma=chroma&&maximum-minimum>=20;
            uint8_t match=camera_excluded(eye_exclusions,eye_exclusion_count,x,y)?0:(chroma?2:(core?1:0));
            eye_multi_mask[(size_t)y * EYE_MULTI_WIDTH + x]=match;
        }
    }
}

static inline struct eye_color_detection
eye_multi_largest_blob(void) {
    struct eye_color_detection result = {0};float best_score=0;
    for (unsigned start = 0; start < EYE_MULTI_PIXELS; ++start) {
        if (!eye_multi_mask[start]) continue;
        unsigned head = 0, tail = 0, area = 0, chroma=0,peak=0;
        unsigned sum_x = 0, sum_y = 0;
        unsigned min_x = EYE_MULTI_WIDTH, min_y = EYE_MULTI_HEIGHT;
        unsigned max_x = 0, max_y = 0;
        eye_multi_queue[tail++] = start;
        if(eye_multi_mask[start]==2)chroma++;
        eye_multi_mask[start] = 0;

        while (head < tail) {
            unsigned at = eye_multi_queue[head++];
            unsigned x = at % EYE_MULTI_WIDTH;
            unsigned y = at / EYE_MULTI_WIDTH;
            ++area;
            size_t raw_at=(size_t)y*1280u+(size_t)x*2u;unsigned value=eye_multi_raw[raw_at+1];if(eye_multi_raw[raw_at+640]>value)value=eye_multi_raw[raw_at+640];unsigned green=((unsigned)eye_multi_raw[raw_at]+eye_multi_raw[raw_at+641])/2;if(green>value)value=green;if(value>peak)peak=value;
            sum_x += x;
            sum_y += y;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;

            unsigned neighbor[4];
            unsigned neighbors = 0;
            if (x) neighbor[neighbors++] = at - 1;
            if (x + 1 < EYE_MULTI_WIDTH) neighbor[neighbors++] = at + 1;
            if (y) neighbor[neighbors++] = at - EYE_MULTI_WIDTH;
            if (y + 1 < EYE_MULTI_HEIGHT) neighbor[neighbors++] = at + EYE_MULTI_WIDTH;
            for (unsigned i = 0; i < neighbors; ++i) {
                unsigned next = neighbor[i];
                if (eye_multi_mask[next]) {
                    if(eye_multi_mask[next]==2)chroma++;
                    eye_multi_mask[next] = 0;
                    eye_multi_queue[tail++] = next;
                }
            }
        }

        unsigned width = max_x - min_x + 1;
        unsigned height = max_y - min_y + 1;
        if (min_x==0 || min_y==0 || max_x+1==EYE_MULTI_WIDTH || max_y+1==EYE_MULTI_HEIGHT || (chroma<2 || chroma*30<area) || peak<(eye_multi_rgb_source?100u:(eye_multi_require_chroma?180u:220u)) || area < 6 || area > (eye_multi_require_chroma?4000u:2000u) || width > (eye_multi_require_chroma?80u:50u) || height > (eye_multi_require_chroma?80u:50u) ||
            width > height * (eye_multi_require_chroma?4u:2u) || height > width * (eye_multi_require_chroma?4u:2u) ||
            area * 100 < width * height * 35)
            continue;
        float compact=(float)(width<height?width:height)/(float)(width>height?width:height);
        float score=(float)chroma*compact*compact;
        /* Peripheral decorative lights need much stronger colored support. */
        if(min_x<24||min_y<20||max_x>=296||max_y>=220)score*=0.05f;
        if(score>=1.0f&&score>best_score){best_score=score;
            result.valid = 1;
            result.area = area;
            result.x = (float)sum_x / (float)area;
            result.y = (float)sum_y / (float)area;
            result.width = (float)width;
            result.height = (float)height;
        }
    }
    return result;
}

/* Returns 1 for a well-formed fixed-size Bayer frame, even if no blobs are
 * found. Returns 0 for malformed input and clears all six output records. */
static inline int eye_detect_colors(const uint8_t *raw, size_t length,
                                    struct eye_color_detection out[EYE_COLOR_COUNT]) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out) * EYE_COLOR_COUNT);
    if (!raw || length != EYE_MULTI_RAW_BYTES) return 0;
    for (unsigned color = 0; color < EYE_COLOR_COUNT; ++color) {
        eye_multi_build_mask(raw, (enum eye_color)color);
        out[color] = eye_multi_largest_blob();
    }
    /* A pink sphere's bright fringe can also satisfy the red/blue masks.
     * Do not publish a second identity at that same physical light. */
    if(out[EYE_COLOR_MAGENTA].valid){const struct eye_color_detection*m=&out[EYE_COLOR_MAGENTA];for(unsigned c=0;c<EYE_COLOR_COUNT;c++)if(c!=EYE_COLOR_MAGENTA&&out[c].valid){float dx=out[c].x-m->x,dy=out[c].y-m->y;float radius=(out[c].width<m->width?out[c].width:m->width)*0.6f+2.0f;if(dx*dx+dy*dy<radius*radius)memset(&out[c],0,sizeof out[c]);}}
    const unsigned extra[2]={EYE_COLOR_ORANGE,EYE_COLOR_VIOLET};
    const unsigned base[2]={EYE_COLOR_RED,EYE_COLOR_BLUE};
    for(unsigned i=0;i<2;i++)if(out[extra[i]].valid&&out[base[i]].valid){float dx=out[extra[i]].x-out[base[i]].x,dy=out[extra[i]].y-out[base[i]].y;if(dx*dx+dy*dy<64)memset(&out[base[i]],0,sizeof out[base[i]]);}
    return 1;
}

#endif
