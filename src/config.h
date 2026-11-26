/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <Limelight.h>

#include <stdbool.h>

#define MAX_INPUTS 6

#ifdef __WIIU__
#define MOONLIGHT_WIIU_PATH "/vol/external01/wiiu/apps/moonlight"
#define MOONLIGHT_WIIU_CONFIG_FILE MOONLIGHT_WIIU_PATH "/moonlight.conf"
#endif

enum codecs { CODEC_UNSPECIFIED, CODEC_H264, CODEC_HEVC, CODEC_AV1 };

typedef enum mouse_modes {
  MOUSE_MODE_RELATIVE,
  MOUSE_MODE_ABSOLUTE,
  MOUSE_MODE_TOUCHSCREEN
} mouse_modes;

typedef struct _CONFIGURATION {
  STREAM_CONFIGURATION stream;
  int debug_level;
  char* app;
  char* action;
  char* address;
  char* mapping;
  char* platform;
  char* audio_device;
  char* config_file;
  char key_dir[4096];
  bool sops;
  bool localaudio;
  bool fullscreen;
  int rotate;
  bool unsupported;
  bool quitappafter;
  bool viewonly;
  bool mouse_emulation;
  char* inputs[MAX_INPUTS];
  int inputsCount;
  enum codecs codec;
  bool hdr;
  int pin;
  unsigned short port;
} CONFIGURATION, *PCONFIGURATION;

extern bool inputAdded;

bool config_file_parse(char* filename, PCONFIGURATION config);
void config_parse(int argc, char* argv[], PCONFIGURATION config);
int config_default_bitrate(int width, int height, int fps);
