/*
 * decode.c - unified audio decoder for the Retro-Go music player.
 *
 * Formats: MP3 (minimp3), WAV (built-in PCM), FLAC (dr_flac), OGG Vorbis (stb_vorbis).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "decode.h"

/* Get declarations only (implementations live in the *_wrap.c files). */
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

#include "minimp3.h"
#include "dr_flac.h"

#define PLAYER_DECODE_MAX_FRAMES 1152

struct decoder {
    decoder_format_t format;
    decoder_info_t info;
    int64_t frame_pos;
    int64_t total_frames;

    union {
        struct {
            mp3dec_t dec;
            const uint8_t *data;
            size_t size;
            size_t offset;
        } mp3;
        struct {
            const uint8_t *data;
            size_t size;
            size_t offset;
            int bytes_per_frame;
        } wav;
        struct {
            drflac *flac;
        } flac;
        struct {
            stb_vorbis *v;
            int channels;
        } ogg;
    } u;
};

/* ------------------ WAV ------------------ */

static int wav_parse_header(const uint8_t *d, size_t size, decoder_info_t *info,
                            size_t *data_offset, size_t *data_size)
{
    if (size < 44 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0)
        return -1;

    size_t p = 12;
    int fmt_found = 0;
    uint16_t fmt_tag = 0, channels = 0, bits = 0;
    uint32_t sample_rate = 0;

    while (p + 8 <= size) {
        uint32_t csize = (uint32_t)(d[p+4] | (d[p+5] << 8) | (d[p+6] << 16) | ((uint32_t)d[p+7] << 24));
        if (memcmp(d + p, "fmt ", 4) == 0 && csize >= 16) {
            fmt_tag   = (uint16_t)(d[p+8] | (d[p+9] << 8));
            channels  = (uint16_t)(d[p+10] | (d[p+11] << 8));
            sample_rate = (uint32_t)(d[p+12] | (d[p+13] << 8) | (d[p+14] << 16) | ((uint32_t)d[p+15] << 24));
            bits      = (uint16_t)(d[p+22] | (d[p+23] << 8));
            fmt_found = 1;
        } else if (memcmp(d + p, "data", 4) == 0) {
            *data_offset = p + 8;
            *data_size = csize;
            if (*data_offset + *data_size > size)
                *data_size = size - *data_offset;
        }
        p += csize + (csize & 1) + 8;
    }
    if (!fmt_found || fmt_tag != 1)
        return -1;

    info->sample_rate = (int)sample_rate;
    info->channels = channels ? channels : 1;
    info->bits_per_sample = bits ? bits : 16;
    return 0;
}

static int wav_read_frames(decoder_t *dec, int16_t *out, int max_frames)
{
    int frames = 0;
    int bytes_per_sample = dec->info.bits_per_sample / 8;
    if (bytes_per_sample < 1) bytes_per_sample = 2;

    while (frames < max_frames && dec->u.wav.offset + dec->u.wav.bytes_per_frame <= dec->u.wav.size) {
        const uint8_t *s = dec->u.wav.data + dec->u.wav.offset;
        if (dec->info.channels == 2) {
            for (int c = 0; c < 2; c++) {
                int16_t v = 0;
                for (int b = 0; b < bytes_per_sample && b < 4; b++)
                    v |= (int16_t)(s[c * bytes_per_sample + b] << (8 * b));
                out[frames * 2 + c] = v;
            }
        } else {
            int16_t v = 0;
            for (int b = 0; b < bytes_per_sample && b < 4; b++)
                v |= (int16_t)(s[b] << (8 * b));
            out[frames * 2 + 0] = v;
            out[frames * 2 + 1] = v;
        }
        dec->u.wav.offset += dec->u.wav.bytes_per_frame;
        frames++;
    }
    return frames;
}

/* ------------------ MP3 ------------------ */

static int mp3_read_frames(decoder_t *dec, int16_t *out, int max_frames)
{
    static mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_frame_info_t fi;

    while (dec->u.mp3.offset < dec->u.mp3.size) {
        int n = mp3dec_decode_frame(&dec->u.mp3.dec, dec->u.mp3.data + dec->u.mp3.offset,
                                    (int)(dec->u.mp3.size - dec->u.mp3.offset), pcm, &fi);
        if (fi.frame_bytes <= 0) { dec->u.mp3.offset++; continue; }
        dec->u.mp3.offset += fi.frame_bytes;
        if (n <= 0) continue;

        int ch = fi.channels > 0 ? fi.channels : dec->info.channels;
        int frames = n / ch;
        if (frames > max_frames) frames = max_frames;

        if (ch == 2) {
            for (int i = 0; i < frames; i++) {
                out[i*2+0] = pcm[i*2+0];
                out[i*2+1] = pcm[i*2+1];
            }
        } else {
            for (int i = 0; i < frames; i++) {
                out[i*2+0] = pcm[i];
                out[i*2+1] = pcm[i];
            }
        }
        return frames;
    }
    return 0; /* EOF */
}

/* ------------------ OGG ------------------ */

static int ogg_read_frames(decoder_t *dec, int16_t *out, int max_frames)
{
    short buf[PLAYER_DECODE_MAX_FRAMES * 2];
    int n = stb_vorbis_get_samples_short_interleaved(dec->u.ogg.v, dec->u.ogg.channels, buf,
                                                      max_frames * dec->u.ogg.channels);
    if (n <= 0) return 0;
    int frames = n / dec->u.ogg.channels;
    if (dec->u.ogg.channels == 2) {
        memcpy(out, buf, sizeof(short) * 2 * frames);
    } else {
        for (int i = 0; i < frames; i++) {
            out[i*2+0] = buf[i];
            out[i*2+1] = buf[i];
        }
    }
    return frames;
}

/* ------------------ FLAC ------------------ */

static int flac_read_frames(decoder_t *dec, int16_t *out, int max_frames)
{
    short buf[PLAYER_DECODE_MAX_FRAMES * 2];
    drflac_uint64 n = drflac_read_pcm_frames_s16(dec->u.flac.flac,
                                                 (drflac_uint64)max_frames, buf);
    if (n <= 0) return 0;
    int frames = (int)n;
    if (dec->info.channels == 2) {
        memcpy(out, buf, sizeof(short) * 2 * frames);
    } else {
        for (int i = 0; i < frames; i++) {
            out[i*2+0] = buf[i];
            out[i*2+1] = buf[i];
        }
    }
    return frames;
}

/* ------------------ public API ------------------ */

decoder_format_t decoder_format_from_ext(const char *ext)
{
    if (!ext) return DECODER_FORMAT_UNKNOWN;
    if (!strcasecmp(ext, "mp3")) return DECODER_FORMAT_MP3;
    if (!strcasecmp(ext, "wav")) return DECODER_FORMAT_WAV;
    if (!strcasecmp(ext, "flac")) return DECODER_FORMAT_FLAC;
    if (!strcasecmp(ext, "ogg")) return DECODER_FORMAT_OGG;
    return DECODER_FORMAT_UNKNOWN;
}

decoder_t *decoder_open(decoder_format_t format, const uint8_t *data, size_t size)
{
    decoder_t *dec = calloc(1, sizeof(decoder_t));
    if (!dec || !data || !size) { free(dec); return NULL; }
    dec->format = format;

    switch (format) {
    case DECODER_FORMAT_WAV: {
        size_t doff = 0, dsz = 0;
        if (wav_parse_header(data, size, &dec->info, &doff, &dsz) != 0) { free(dec); return NULL; }
        dec->u.wav.data = data + doff;
        dec->u.wav.size = dsz;
        dec->u.wav.offset = 0;
        dec->u.wav.bytes_per_frame = (dec->info.bits_per_sample / 8) * dec->info.channels;
        if (dec->u.wav.bytes_per_frame < 1) { free(dec); return NULL; }
        dec->total_frames = dec->u.wav.size / dec->u.wav.bytes_per_frame;
        break;
    }
    case DECODER_FORMAT_MP3: {
        mp3dec_init(&dec->u.mp3.dec);
        dec->u.mp3.data = data;
        dec->u.mp3.size = size;
        dec->u.mp3.offset = 0;
        mp3dec_frame_info_t fi;
        mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        mp3dec_decode_frame(&dec->u.mp3.dec, data, (int)size, pcm, &fi);
        dec->info.sample_rate = fi.hz > 0 ? fi.hz : 44100;
        dec->info.channels = fi.channels > 0 ? fi.channels : 2;
        dec->info.bits_per_sample = 16;
        mp3dec_init(&dec->u.mp3.dec);
        dec->total_frames = -1;
        break;
    }
    case DECODER_FORMAT_FLAC: {
        drflac *f = drflac_open_memory(data, size, NULL);
        if (!f) { free(dec); return NULL; }
        dec->u.flac.flac = f;
        dec->info.sample_rate = (int)f->sampleRate;
        dec->info.channels = f->channels ? f->channels : 2;
        dec->info.bits_per_sample = f->bitsPerSample ? f->bitsPerSample : 16;
        dec->total_frames = (int64_t)f->totalPCMFrameCount;
        break;
    }
    case DECODER_FORMAT_OGG: {
        int err = 0;
        stb_vorbis *v = stb_vorbis_open_memory((const unsigned char *)data,
                                               (int)size, &err, NULL);
        if (!v) { free(dec); return NULL; }
        stb_vorbis_info oi = stb_vorbis_get_info(v);
        dec->u.ogg.v = v;
        dec->u.ogg.channels = oi.channels > 0 ? oi.channels : 2;
        dec->info.sample_rate = (int)oi.sample_rate;
        dec->info.channels = oi.channels > 0 ? oi.channels : 2;
        dec->info.bits_per_sample = 16;
        dec->total_frames = -1;
        break;
    }
    default:
        free(dec);
        return NULL;
    }

    dec->frame_pos = 0;
    return dec;
}

bool decoder_get_info(decoder_t *dec, decoder_info_t *info)
{
    if (!dec || !info) return false;
    *info = dec->info;
    return true;
}

int decoder_read_frames(decoder_t *dec, int16_t *out, int max_frames)
{
    if (!dec || !out || max_frames <= 0) return 0;
    int n = 0;
    switch (dec->format) {
    case DECODER_FORMAT_WAV:  n = wav_read_frames(dec, out, max_frames); break;
    case DECODER_FORMAT_MP3:  n = mp3_read_frames(dec, out, max_frames); break;
    case DECODER_FORMAT_FLAC: n = flac_read_frames(dec, out, max_frames); break;
    case DECODER_FORMAT_OGG:  n = ogg_read_frames(dec, out, max_frames); break;
    default: return 0;
    }
    if (n > 0) dec->frame_pos += n;
    return n;
}

int64_t decoder_seek(decoder_t *dec, int64_t frame_pos)
{
    if (!dec) return -1;
    switch (dec->format) {
    case DECODER_FORMAT_WAV:
        dec->u.wav.offset = (size_t)frame_pos * dec->u.wav.bytes_per_frame;
        if (dec->u.wav.offset > dec->u.wav.size) dec->u.wav.offset = dec->u.wav.size;
        dec->frame_pos = frame_pos;
        return frame_pos;
    case DECODER_FORMAT_MP3:
        return -1; /* MP3 seek not supported in this implementation */
    case DECODER_FORMAT_FLAC:
        if (drflac_seek_to_pcm_frame(dec->u.flac.flac, (drflac_uint64)frame_pos)) {
            dec->frame_pos = frame_pos;
            return frame_pos;
        }
        return -1;
    case DECODER_FORMAT_OGG:
        if (stb_vorbis_seek(dec->u.ogg.v, (unsigned int)frame_pos)) {
            dec->frame_pos = frame_pos;
            return frame_pos;
        }
        return -1;
    default:
        return -1;
    }
}

int64_t decoder_total_frames(decoder_t *dec)
{
    return dec ? dec->total_frames : -1;
}

void decoder_close(decoder_t *dec)
{
    if (!dec) return;
    switch (dec->format) {
    case DECODER_FORMAT_FLAC:
        if (dec->u.flac.flac) drflac_close(dec->u.flac.flac);
        break;
    case DECODER_FORMAT_OGG:
        if (dec->u.ogg.v) stb_vorbis_close(dec->u.ogg.v);
        break;
    default:
        break;
    }
    free(dec);
}
