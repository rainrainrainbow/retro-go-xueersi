/*
 * player/main.c - Retro-Go music player application
 *
 * Plays audio files (MP3/WAV/FLAC/OGG) through the I2S audio output.
 */

#include <rg_system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decode.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_FRAMES 1024

static rg_app_t *app;
static decoder_t *decoder = NULL;
static int16_t *audio_buffer = NULL;
static bool paused = false;
static bool playing = false;

static void audio_task(void *arg)
{
    decoder_info_t info;
    if (!decoder_get_info(decoder, &info)) {
        RG_LOGE("Failed to get decoder info");
        rg_system_exit();
        return;
    }

    RG_LOGI("Audio task started: %d Hz, %d channels, %d bits",
            info.sample_rate, info.channels, info.bits_per_sample);

    /* Set audio sample rate to match the decoded file */
    rg_audio_set_sample_rate(info.sample_rate);

    playing = true;

    while (playing) {
        if (paused) {
            rg_task_delay(100);
            continue;
        }

        int frames = decoder_read_frames(decoder, audio_buffer, AUDIO_BUFFER_FRAMES);
        if (frames <= 0) {
            /* End of file */
            RG_LOGI("Playback finished");
            playing = false;
            break;
        }

        rg_audio_submit(audio_buffer, frames);
        rg_system_tick(0);
    }

    rg_system_exit();
}

static void event_handler(int event, void *arg)
{
    if (event == RG_EVENT_REDRAW) {
        /* Could update display here if needed */
    }
}

static void input_handler(void)
{
    uint32_t keys = rg_input_read_gamepad();

    if (keys & RG_KEY_START) {
        /* Toggle pause */
        paused = !paused;
        RG_LOGI("Playback %s", paused ? "paused" : "resumed");
        rg_input_wait_for_key(RG_KEY_START, false, 500);
    }

    if (keys & RG_KEY_B) {
        /* Stop playback */
        playing = false;
        RG_LOGI("Stopping playback");
    }
}

void app_main(void)
{
    const rg_handlers_t handlers = {
        .event = &event_handler,
    };

    app = rg_system_init(AUDIO_SAMPLE_RATE, &handlers, NULL);

    RG_LOGI("Music player started");
    RG_LOGI("ROM path: %s", app->romPath);

    /* Get file extension */
    const char *ext = strrchr(app->romPath, '.');
    if (!ext) {
        rg_gui_alert("Error", "No file extension");
        rg_system_exit();
        return;
    }
    ext++; /* skip the dot */

    /* Determine format */
    decoder_format_t format = decoder_format_from_ext(ext);
    if (format == DECODER_FORMAT_UNKNOWN) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Unsupported format: %s", ext);
        rg_gui_alert("Error", msg);
        rg_system_exit();
        return;
    }

    /* Load entire file into memory */
    void *file_data = NULL;
    size_t file_size = 0;
    if (!rg_storage_read_file(app->romPath, &file_data, &file_size, 0)) {
        rg_gui_alert("Error", "Failed to read file");
        rg_system_exit();
        return;
    }

    RG_LOGI("Loaded file: %zu bytes", file_size);

    /* Open decoder */
    decoder = decoder_open(format, file_data, file_size);
    if (!decoder) {
        free(file_data);
        rg_gui_alert("Error", "Failed to open decoder");
        rg_system_exit();
        return;
    }

    /* Allocate audio buffer */
    audio_buffer = malloc(AUDIO_BUFFER_FRAMES * 2 * sizeof(int16_t));
    if (!audio_buffer) {
        decoder_close(decoder);
        free(file_data);
        rg_gui_alert("Error", "Out of memory");
        rg_system_exit();
        return;
    }

    /* Show brief info */
    decoder_info_t info;
    decoder_get_info(decoder, &info);
    char info_msg[128];
    snprintf(info_msg, sizeof(info_msg),
             "Format: %s\nRate: %d Hz\nChannels: %d\n\nSTART: Pause\nB: Stop",
             ext, info.sample_rate, info.channels);
    rg_gui_alert("Now Playing", info_msg);

    /* Start audio task */
    rg_task_create("audio_task", audio_task, NULL, 8192, RG_TASK_PRIORITY_2, 0);

    /* Main loop - handle input */
    while (playing) {
        input_handler();
        rg_task_delay(50);
    }

    /* Cleanup */
    free(audio_buffer);
    decoder_close(decoder);
    free(file_data);

    RG_LOGI("Music player exiting");
    rg_system_exit();
}
