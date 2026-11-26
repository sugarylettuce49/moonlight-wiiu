#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define VERSION_STRING "v1.6"

enum {
  STATE_INVALID,
  STATE_DISCONNECTED,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_PAIRING,
  STATE_START_STREAM,
  STATE_STOP_STREAM,
  STATE_STREAMING,
  STATE_SETTINGS,
};

#include <Limelight.h>
#include <stdint.h>
#include <stdbool.h>
#include "font.h"

#include <gx2/texture.h>

extern int state;
extern int is_error;
extern char message_buffer[1024];

#define FRAME_BUFFER 12

void wiiu_stream_init(uint32_t width, uint32_t height);
void wiiu_stream_draw(void);
void wiiu_stream_fini(void);
void wiiu_setup_renderstate(void);

// Number of decode target textures. The renderer always shows the newest
// decoded frame, so the decoder has to be able to run a few frames ahead
// without overwriting the texture the GPU is currently scanning out.
#define NUM_BUFFERS 4
#define MAX_QUEUEMESSAGES 16

typedef struct {
  GX2Texture yTex;
  GX2Texture uvTex;
} yuv_texture_t;

void* get_frame(void);
void add_frame(yuv_texture_t* msg);
void reset_frame_queue(void);
void wiiu_stream_set_video_size(uint32_t height, uint32_t texture_height);

extern AUDIO_RENDERER_CALLBACKS audio_callbacks_wiiu;
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_wiiu;

// input
void wiiu_input_init(void);
void wiiu_input_update(void); // this is only relevant while streaming
uint32_t wiiu_input_num_controllers(void);
uint32_t wiiu_input_buttons_triggered(void); // only really used for the menu
uint32_t wiiu_input_buttons_held(void); // buttons held during the last wiiu_input_buttons_triggered call
void start_input_thread(void);
void stop_input_thread(void);

// proc
void wiiu_proc_init(void);
void wiiu_proc_shutdown(void);
void wiiu_proc_register_home_callback(void);
int wiiu_proc_running(void);
void wiiu_proc_stop_running(void);
void wiiu_proc_set_home_enabled(int enabled);

// net
void wiiu_net_init(void);
void wiiu_net_shutdown(void);
