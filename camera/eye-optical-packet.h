/* SPDX-License-Identifier: GPL-3.0-or-later
 * Small wire-format helper for the independent PS3 Eye optical stream.
 */
#ifndef EYE_OPTICAL_PACKET_H
#define EYE_OPTICAL_PACKET_H

#include "eye-multisphere.h"
#include <stdint.h>
#include <string.h>

#define EYE_OPTICAL_MAGIC UINT32_C(0x31455945) /* bytes "EYE1" */
#define EYE_OPTICAL_VERSION 2u
#define EYE_OPTICAL_PACKET_BYTES 224u
#define EYE_OPTICAL_PORT 9990u

static inline void eye_optical_put_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static inline void eye_optical_put_le64(uint8_t *out, uint64_t value)
{
    eye_optical_put_le32(out, (uint32_t)value);
    eye_optical_put_le32(out + 4, (uint32_t)(value >> 32));
}

static inline void eye_optical_put_le_float(uint8_t *out, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    eye_optical_put_le32(out, bits);
}

static inline void eye_optical_packet_build(
    uint8_t packet[EYE_OPTICAL_PACKET_BYTES], uint64_t sequence,
    uint64_t monotonic_us,
    const struct eye_color_detection detections[EYE_COLOR_COUNT])
{
    memset(packet, 0, EYE_OPTICAL_PACKET_BYTES);
    eye_optical_put_le32(packet, EYE_OPTICAL_MAGIC);
    packet[4] = (uint8_t)EYE_OPTICAL_VERSION;
    packet[6] = (uint8_t)EYE_OPTICAL_PACKET_BYTES;
    packet[7] = (uint8_t)(EYE_OPTICAL_PACKET_BYTES >> 8);
    eye_optical_put_le64(packet + 8, sequence);
    eye_optical_put_le64(packet + 16, monotonic_us);
    eye_optical_put_le32(packet + 24, EYE_MULTI_WIDTH);
    eye_optical_put_le32(packet + 28, EYE_MULTI_HEIGHT);

    for (unsigned color = 0; color < EYE_COLOR_COUNT; ++color) {
        const struct eye_color_detection *d = &detections[color];
        uint8_t *entry = packet + 32u + color * 24u;
        eye_optical_put_le32(entry, d->valid ? 1u : 0u);
        eye_optical_put_le32(entry + 4, d->valid ? d->area : 0u);
        eye_optical_put_le_float(entry + 8, d->valid ? d->x : 0.0f);
        eye_optical_put_le_float(entry + 12, d->valid ? d->y : 0.0f);
        eye_optical_put_le_float(entry + 16, d->valid ? d->width : 0.0f);
        eye_optical_put_le_float(entry + 20, d->valid ? d->height : 0.0f);
    }
}

#endif
