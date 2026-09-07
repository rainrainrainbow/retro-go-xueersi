#ifndef PLAYER_DECODE_H
#define PLAYER_DECODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported audio formats */
typedef enum {
    DECODER_FORMAT_UNKNOWN = 0,
    DECODER_FORMAT_MP3,
    DECODER_FORMAT_WAV,
    DECODER_FORMAT_FLAC,
    DECODER_FORMAT_OGG,
} decoder_format_t;

typedef struct decoder decoder_t;

typedef struct {
    int sample_rate;      /* 44100 etc */
    int channels;         /* 1 or 2 */
    int bits_per_sample;  /* 16 */
} decoder_info_t;

/* Open a decoder for the given in-memory buffer.
 * The buffer must remain valid for the lifetime of the decoder. */
decoder_t *decoder_open(decoder_format_t format, const uint8_t *data, size_t size);

/* Return format info. Returns false if unknown. */
bool decoder_get_info(decoder_t *dec, decoder_info_t *info);

/* Decode up to `frames` interleaved stereo PCM frames (L/R int16 per frame).
 * Returns the number of frames actually decoded (0 = EOF).
 * Note: mono input is up-mixed to stereo by the caller. */
int decoder_read_frames(decoder_t *dec, int16_t *out, int max_frames);

/* Seek to the given PCM frame position (in decoded frames). Returns new position or -1. */
int64_t decoder_seek(decoder_t *dec, int64_t frame_pos);

/* Total number of PCM frames if known, else -1. */
int64_t decoder_total_frames(decoder_t *dec);

/* Close and free the decoder. */
void decoder_close(decoder_t *dec);

/* Map a filename extension to a decoder format. Returns DECODER_FORMAT_UNKNOWN if unsupported. */
decoder_format_t decoder_format_from_ext(const char *ext);

#ifdef __cplusplus
}
#endif

#endif /* PLAYER_DECODE_H */
