/* minimp3 (public domain) wrapper */
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

/* Provide a thin streaming-style helper for MP3.
 * We keep it minimal; the main decode.c drives mp3dec_decode_frame directly. */
