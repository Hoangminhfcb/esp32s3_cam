#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* GCV1: one request byte -> 20-byte header -> exactly jpeg_bytes bytes.
 * All integers are big endian. Never transmit a C struct (padding/endian).
 * One outstanding request prevents a queue of old images inside TCP.
 * Keep this file identical in both independently buildable projects. */
#define VIDEO_REQUEST_NEXT 1
#define VIDEO_HEADER_BYTES 20
#define VIDEO_WIRE_MAX_JPEG (64U * 1024U)
#define VIDEO_WIRE_WIDTH 320
#define VIDEO_WIRE_HEIGHT 240

typedef struct {
    uint32_t jpeg_bytes;
    uint32_t sequence;
    uint32_t capture_age_us; /* measured entirely on the camera clock */
    uint16_t width, height;
} video_header_t;

static inline void video_put_u32(uint8_t *p, uint32_t n)
{
    p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}
static inline uint32_t video_get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static inline void video_header_encode(uint8_t *out, const video_header_t *h)
{
    memcpy(out, "GCV1", 4);
    video_put_u32(out + 4, h->jpeg_bytes);
    video_put_u32(out + 8, h->sequence);
    video_put_u32(out + 12, h->capture_age_us);
    out[16] = h->width >> 8; out[17] = h->width;
    out[18] = h->height >> 8; out[19] = h->height;
}
static inline bool video_header_decode(const uint8_t *in, size_t length, video_header_t *h)
{
    if (length != VIDEO_HEADER_BYTES || memcmp(in, "GCV1", 4)) return false;
    h->jpeg_bytes = video_get_u32(in + 4);
    h->sequence = video_get_u32(in + 8);
    h->capture_age_us = video_get_u32(in + 12);
    h->width = ((uint16_t)in[16] << 8) | in[17];
    h->height = ((uint16_t)in[18] << 8) | in[19];
    return h->jpeg_bytes >= 4 && h->jpeg_bytes <= VIDEO_WIRE_MAX_JPEG &&
           h->width == VIDEO_WIRE_WIDTH && h->height == VIDEO_WIRE_HEIGHT;
}
