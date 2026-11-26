// On-console editor for moonlight.conf, so settings can be changed without
// editing the file on the SD card.
#include "settings.h"
#include "wiiu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include <vpad/input.h>

extern int disable_gamepad;
extern int swap_buttons;
extern mouse_modes mouse_mode;
extern int autostream;

#define MAX_APPS 64

#define MENU_X_CURSOR   24
#define MENU_X_LABEL    64
#define MENU_X_VALUE    760
#define MENU_Y_TITLE    58
#define MENU_Y_FIRST    150
#define MENU_LINE       56
#define MENU_Y_DESC     990
#define MENU_Y_HELP     1050

// frames a direction has to be held before it starts repeating, and the repeat interval
#define REPEAT_DELAY    20
#define REPEAT_INTERVAL 4

#define BITRATE_MIN       500
#define BITRATE_MAX       50000
#define BITRATE_STEP      500
#define BITRATE_BIG_STEP  5000

enum {
  ITEM_ADDRESS,
  ITEM_APP,
  ITEM_RESOLUTION,
  ITEM_FPS,
  ITEM_BITRATE,
  ITEM_REMOTE,
  ITEM_MOUSE_MODE,
  ITEM_DISABLE_GAMEPAD,
  ITEM_SWAP_BUTTONS,
  ITEM_AUTOSTREAM,
  ITEM_QUITAPPAFTER,
  ITEM_LOCALAUDIO,
  ITEM_SAVE,
  ITEM_CANCEL,
  ITEM_COUNT
};

static const char* item_labels[ITEM_COUNT] = {
  [ITEM_ADDRESS]         = "Host address",
  [ITEM_APP]             = "App",
  [ITEM_RESOLUTION]      = "Resolution",
  [ITEM_FPS]             = "Frame rate",
  [ITEM_BITRATE]         = "Bitrate",
  [ITEM_REMOTE]          = "Network tuning",
  [ITEM_MOUSE_MODE]      = "Touchscreen mode",
  [ITEM_DISABLE_GAMEPAD] = "Disable GamePad",
  [ITEM_SWAP_BUTTONS]    = "Swap A/B and X/Y",
  [ITEM_AUTOSTREAM]      = "Auto-stream on launch",
  [ITEM_QUITAPPAFTER]    = "Quit app after stream",
  [ITEM_LOCALAUDIO]      = "Play audio on host",
  [ITEM_SAVE]            = "Save and go back",
  [ITEM_CANCEL]          = "Discard changes",
};

static const char* item_descriptions[ITEM_COUNT] = {
  [ITEM_ADDRESS]         = "IP address of the host PC. Press  to edit it.",
  [ITEM_APP]             = "App to start on the host. Use left/right to pick from the host's list.",
  [ITEM_RESOLUTION]      = "Stream resolution. Higher resolutions need a stronger connection.",
  [ITEM_FPS]             = "60 fps is smoother but needs roughly twice the bandwidth.",
  [ITEM_BITRATE]         = "Lower this if the stream stutters or breaks up. L/R: steps of 5 Mbps.",
  [ITEM_REMOTE]          = "LAN or internet streaming optimizations. Auto decides from the address.",
  [ITEM_MOUSE_MODE]      = "How touches on the GamePad screen are sent to the host.",
  [ITEM_DISABLE_GAMEPAD] = "Don't use the GamePad as a controller, other controllers become 1-4.",
  [ITEM_SWAP_BUTTONS]    = "Swap the face buttons to match the Xbox controller layout.",
  [ITEM_AUTOSTREAM]      = "Connect and start streaming as soon as Moonlight launches.",
  [ITEM_QUITAPPAFTER]    = "Close the app on the host when the stream ends.",
  [ITEM_LOCALAUDIO]      = "Play the audio on the host PC instead of the Wii U.",
  [ITEM_SAVE]            = "Apply the changes and write them to moonlight.conf on the SD card.",
  [ITEM_CANCEL]          = "Leave the settings without changing anything.",
};

typedef struct {
  int width;
  int height;
  const char* name;
} resolution_t;

static const resolution_t resolutions[] = {
  { 640,  360,  "640x360" },
  { 854,  480,  "854x480" },
  { 1280, 720,  "1280x720 (720p)" },
  { 1920, 1080, "1920x1080 (1080p)" },
};
#define NUM_RESOLUTIONS (sizeof(resolutions) / sizeof(resolutions[0]))

static const int fps_options[] = { 30, 60 };
#define NUM_FPS_OPTIONS (sizeof(fps_options) / sizeof(fps_options[0]))

typedef struct {
  char address[64];
  char app[256];
  int width;
  int height;
  int fps;
  int bitrate; // -1 = auto
  int remote;  // STREAM_CFG_*
  int mouse_mode;
  bool disable_gamepad;
  bool swap_buttons;
  bool autostream;
  bool quitappafter;
  bool localaudio;
} settings_values_t;

static settings_values_t values;
static int selected;

static char* app_names[MAX_APPS];
static int app_count;

static bool editing_address;
static int address_octets[4];
static int address_cursor;

static int repeat_frames[4];

static void free_apps(void)
{
  for (int i = 0; i < app_count; i++) {
    free(app_names[i]);
    app_names[i] = NULL;
  }
  app_count = 0;
}

static void add_app(const char* name)
{
  if (!name || app_count >= MAX_APPS)
    return;

  for (int i = 0; i < app_count; i++) {
    if (strcmp(app_names[i], name) == 0)
      return;
  }

  app_names[app_count] = strdup(name);
  if (app_names[app_count])
    app_count++;
}

void settings_open(PCONFIGURATION config, PAPP_LIST apps)
{
  memset(&values, 0, sizeof(values));

  if (config->address)
    snprintf(values.address, sizeof(values.address), "%s", config->address);
  snprintf(values.app, sizeof(values.app), "%s", config->app ? config->app : "Steam");

  values.width = config->stream.width;
  values.height = config->stream.height;
  values.fps = config->stream.fps;
  values.bitrate = config->stream.bitrate > 0 ? config->stream.bitrate : -1;
  values.remote = config->stream.streamingRemotely;
  values.mouse_mode = mouse_mode;
  values.disable_gamepad = disable_gamepad;
  values.swap_buttons = swap_buttons;
  values.autostream = autostream;
  values.quitappafter = config->quitappafter;
  values.localaudio = config->localaudio;

  // the currently configured app always comes first so it stays selectable
  free_apps();
  add_app(values.app);

  // the list is built in reverse order by the xml parser, collect it first
  PAPP_LIST reversed[MAX_APPS];
  int n = 0;
  while (apps != NULL) {
    PAPP_LIST next = apps->next;
    if (n < MAX_APPS)
      reversed[n++] = apps;
    else {
      free(apps->name);
      free(apps);
    }
    apps = next;
  }
  for (int i = n - 1; i >= 0; i--) {
    add_app(reversed[i]->name);
    free(reversed[i]->name);
    free(reversed[i]);
  }

  selected = 0;
  editing_address = false;
  memset(repeat_frames, 0, sizeof(repeat_frames));
}

// Adds auto-repeat to the d-pad while it's held, and maps the left stick to the d-pad
static uint32_t apply_repeat(uint32_t triggered, uint32_t held)
{
  static const uint32_t dirs[4] = { VPAD_BUTTON_UP, VPAD_BUTTON_DOWN, VPAD_BUTTON_LEFT, VPAD_BUTTON_RIGHT };
  static const uint32_t stick[4] = { VPAD_STICK_L_EMULATION_UP, VPAD_STICK_L_EMULATION_DOWN,
                                     VPAD_STICK_L_EMULATION_LEFT, VPAD_STICK_L_EMULATION_RIGHT };

  for (int i = 0; i < 4; i++) {
    if (triggered & stick[i])
      triggered |= dirs[i];
    if (held & stick[i])
      held |= dirs[i];
  }

  for (int i = 0; i < 4; i++) {
    if (held & dirs[i]) {
      repeat_frames[i]++;
      if (repeat_frames[i] > REPEAT_DELAY && (repeat_frames[i] - REPEAT_DELAY) % REPEAT_INTERVAL == 0)
        triggered |= dirs[i];
    } else {
      repeat_frames[i] = 0;
    }
  }

  return triggered;
}

static int find_resolution(void)
{
  for (int i = 0; i < (int) NUM_RESOLUTIONS; i++) {
    if (resolutions[i].width == values.width && resolutions[i].height == values.height)
      return i;
  }
  return -1;
}

static int find_fps(void)
{
  for (int i = 0; i < (int) NUM_FPS_OPTIONS; i++) {
    if (fps_options[i] == values.fps)
      return i;
  }
  return -1;
}

static int find_app(void)
{
  for (int i = 0; i < app_count; i++) {
    if (strcmp(app_names[i], values.app) == 0)
      return i;
  }
  return -1;
}

static int wrap(int value, int count)
{
  return ((value % count) + count) % count;
}

static void change_value(int item, int dir, bool big_step)
{
  switch (item) {
    case ITEM_APP: {
      if (app_count > 0) {
        int idx = find_app();
        idx = idx < 0 ? 0 : wrap(idx + dir, app_count);
        snprintf(values.app, sizeof(values.app), "%s", app_names[idx]);
      }
      break;
    }
    case ITEM_RESOLUTION: {
      int idx = find_resolution();
      if (idx < 0) {
        // custom resolution from the config file, jump to the closest preset
        idx = dir > 0 ? 0 : NUM_RESOLUTIONS - 1;
        for (int i = 0; i < (int) NUM_RESOLUTIONS; i++) {
          if (resolutions[i].width * resolutions[i].height > values.width * values.height) {
            idx = dir > 0 ? i : (i > 0 ? i - 1 : 0);
            break;
          }
        }
      } else {
        idx = wrap(idx + dir, NUM_RESOLUTIONS);
      }
      values.width = resolutions[idx].width;
      values.height = resolutions[idx].height;
      break;
    }
    case ITEM_FPS: {
      int idx = find_fps();
      idx = idx < 0 ? 0 : wrap(idx + dir, NUM_FPS_OPTIONS);
      values.fps = fps_options[idx];
      break;
    }
    case ITEM_BITRATE: {
      int step = big_step ? BITRATE_BIG_STEP : BITRATE_STEP;
      if (values.bitrate <= 0) {
        // leaving auto starts from what auto would have picked
        if (dir > 0)
          values.bitrate = config_default_bitrate(values.width, values.height, values.fps);
        break;
      }
      // snap odd values from the config file (e.g. 4096) onto the step grid
      int next;
      if (values.bitrate % step != 0)
        next = dir > 0 ? (values.bitrate / step + 1) * step : (values.bitrate / step) * step;
      else
        next = values.bitrate + dir * step;
      if (next < BITRATE_MIN)
        values.bitrate = -1; // going below the minimum switches back to auto
      else if (next > BITRATE_MAX)
        values.bitrate = BITRATE_MAX;
      else
        values.bitrate = next;
      break;
    }
    case ITEM_REMOTE: {
      static const int order[3] = { STREAM_CFG_AUTO, STREAM_CFG_LOCAL, STREAM_CFG_REMOTE };
      int idx = 0;
      for (int i = 0; i < 3; i++) {
        if (order[i] == values.remote)
          idx = i;
      }
      values.remote = order[wrap(idx + dir, 3)];
      break;
    }
    case ITEM_MOUSE_MODE:
      values.mouse_mode = wrap(values.mouse_mode + dir, 3);
      break;
    case ITEM_DISABLE_GAMEPAD:
      values.disable_gamepad = !values.disable_gamepad;
      break;
    case ITEM_SWAP_BUTTONS:
      values.swap_buttons = !values.swap_buttons;
      break;
    case ITEM_AUTOSTREAM:
      values.autostream = !values.autostream;
      break;
    case ITEM_QUITAPPAFTER:
      values.quitappafter = !values.quitappafter;
      break;
    case ITEM_LOCALAUDIO:
      values.localaudio = !values.localaudio;
      break;
    default:
      break;
  }
}

static void begin_address_edit(void)
{
  int a, b, c, d;
  if (sscanf(values.address, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
      a >= 0 && a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255) {
    address_octets[0] = a;
    address_octets[1] = b;
    address_octets[2] = c;
    address_octets[3] = d;
  } else {
    // not set or a hostname, start from a typical home network address
    address_octets[0] = 192;
    address_octets[1] = 168;
    address_octets[2] = 1;
    address_octets[3] = 2;
  }
  address_cursor = 3;
  editing_address = true;
}

static void end_address_edit(void)
{
  snprintf(values.address, sizeof(values.address), "%d.%d.%d.%d",
           address_octets[0], address_octets[1], address_octets[2], address_octets[3]);
  editing_address = false;
}

static void format_value(int item, char* buf, size_t size)
{
  static const char* mouse_names[3] = { "Relative (touchpad)", "Absolute", "Touchscreen" };
  buf[0] = '\0';

  switch (item) {
    case ITEM_ADDRESS:
      if (editing_address) {
        char part[4][8];
        for (int i = 0; i < 4; i++) {
          if (i == address_cursor)
            snprintf(part[i], sizeof(part[i]), "[%d]", address_octets[i]);
          else
            snprintf(part[i], sizeof(part[i]), "%d", address_octets[i]);
        }
        snprintf(buf, size, "%s.%s.%s.%s", part[0], part[1], part[2], part[3]);
      } else {
        snprintf(buf, size, "%s", values.address[0] ? values.address : "(not set)");
      }
      break;
    case ITEM_APP:
      snprintf(buf, size, "%s", values.app);
      break;
    case ITEM_RESOLUTION: {
      int idx = find_resolution();
      if (idx >= 0)
        snprintf(buf, size, "%s", resolutions[idx].name);
      else
        snprintf(buf, size, "%dx%d (custom)", values.width, values.height);
      break;
    }
    case ITEM_FPS:
      snprintf(buf, size, "%d fps", values.fps);
      break;
    case ITEM_BITRATE:
      if (values.bitrate <= 0)
        snprintf(buf, size, "Auto (%.1f Mbps)", config_default_bitrate(values.width, values.height, values.fps) / 1000.0);
      else
        snprintf(buf, size, "%.1f Mbps", values.bitrate / 1000.0);
      break;
    case ITEM_REMOTE:
      snprintf(buf, size, "%s", values.remote == STREAM_CFG_LOCAL ? "LAN" :
                                values.remote == STREAM_CFG_REMOTE ? "Internet" : "Auto");
      break;
    case ITEM_MOUSE_MODE:
      snprintf(buf, size, "%s", mouse_names[wrap(values.mouse_mode, 3)]);
      break;
    case ITEM_DISABLE_GAMEPAD:
      snprintf(buf, size, "%s", values.disable_gamepad ? "On" : "Off");
      break;
    case ITEM_SWAP_BUTTONS:
      snprintf(buf, size, "%s", values.swap_buttons ? "On" : "Off");
      break;
    case ITEM_AUTOSTREAM:
      snprintf(buf, size, "%s", values.autostream ? "On" : "Off");
      break;
    case ITEM_QUITAPPAFTER:
      snprintf(buf, size, "%s", values.quitappafter ? "On" : "Off");
      break;
    case ITEM_LOCALAUDIO:
      snprintf(buf, size, "%s", values.localaudio ? "On" : "Off");
      break;
    default:
      break;
  }
}

static void draw_menu(void)
{
  char value[300];

  Font_Clear();

  Font_SetSize(50);
  Font_SetColor(255, 255, 255, 255);
  Font_Print(8, MENU_Y_TITLE, "Moonlight Wii U " VERSION_STRING " Settings");

  Font_SetSize(40);
  for (int i = 0; i < ITEM_COUNT; i++) {
    uint32_t y = MENU_Y_FIRST + i * MENU_LINE;
    // leave a small gap before the save/discard actions
    if (i >= ITEM_SAVE)
      y += MENU_LINE / 2;

    if (i == selected) {
      Font_SetColor(255, 220, 0, 255);
      Font_Print(MENU_X_CURSOR, y, ">");
    } else {
      Font_SetColor(255, 255, 255, 255);
    }

    Font_Print(MENU_X_LABEL, y, item_labels[i]);

    format_value(i, value, sizeof(value));
    if (value[0]) {
      if (i != selected)
        Font_SetColor(170, 200, 255, 255);
      Font_Print(MENU_X_VALUE, y, value);
    }
  }

  Font_SetSize(32);
  Font_SetColor(200, 200, 200, 255);
  if (editing_address) {
    Font_Print(MENU_X_LABEL, MENU_Y_DESC, "Up/down: change number  L/R: change by 10");
    Font_Print(MENU_X_LABEL, MENU_Y_HELP, "Left/right: move between parts  /: done");
  } else {
    if (selected == ITEM_APP && app_count <= 1)
      Font_Print(MENU_X_LABEL, MENU_Y_DESC, "Connect to the host first to choose from its app list.");
    else
      Font_Print(MENU_X_LABEL, MENU_Y_DESC, item_descriptions[selected]);
    Font_Print(MENU_X_LABEL, MENU_Y_HELP, "Up/down: select  Left/right: change  : select  +: save  : discard");
  }

  Font_Draw_TVDRC();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

enum {
  KEY_ADDRESS,
  KEY_APP,
  KEY_WIDTH,
  KEY_HEIGHT,
  KEY_FPS,
  KEY_BITRATE,
  KEY_REMOTE,
  KEY_MOUSE_MODE,
  KEY_DISABLE_GAMEPAD,
  KEY_SWAP_BUTTONS,
  KEY_AUTOSTREAM,
  KEY_QUITAPPAFTER,
  KEY_LOCALAUDIO,
  KEY_COUNT
};

static const char* config_keys[KEY_COUNT] = {
  [KEY_ADDRESS]         = "address",
  [KEY_APP]             = "app",
  [KEY_WIDTH]           = "width",
  [KEY_HEIGHT]          = "height",
  [KEY_FPS]             = "fps",
  [KEY_BITRATE]         = "bitrate",
  [KEY_REMOTE]          = "remote",
  [KEY_MOUSE_MODE]      = "mouse_mode",
  [KEY_DISABLE_GAMEPAD] = "disable_gamepad",
  [KEY_SWAP_BUTTONS]    = "swap_buttons",
  [KEY_AUTOSTREAM]      = "autostream",
  [KEY_QUITAPPAFTER]    = "quitappafter",
  [KEY_LOCALAUDIO]      = "localaudio",
};

// Returns the managed key on this line, or -1. Matches active lines
// ("key = value") as well as commented-out options ("#key = value"), but
// not documentation lines starting with "##".
static int match_config_line(const char* line, size_t len, bool* commented)
{
  size_t i = 0;
  *commented = false;

  while (i < len && (line[i] == ' ' || line[i] == '\t'))
    i++;

  if (i < len && line[i] == '#') {
    *commented = true;
    i++;
    if (i < len && line[i] == '#')
      return -1;
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
      i++;
  }

  size_t start = i;
  while (i < len && (isalnum((unsigned char) line[i]) || line[i] == '_'))
    i++;
  size_t keylen = i - start;
  if (keylen == 0)
    return -1;

  while (i < len && (line[i] == ' ' || line[i] == '\t'))
    i++;
  if (i >= len || line[i] != '=')
    return -1;

  for (int k = 0; k < KEY_COUNT; k++) {
    if (strlen(config_keys[k]) == keylen && strncmp(config_keys[k], line + start, keylen) == 0)
      return k;
  }
  return -1;
}

static void format_config_value(int key, char* buf, size_t size)
{
  static const char* mouse_values[3] = { "relative", "absolute", "touchscreen" };

  switch (key) {
    case KEY_ADDRESS:         snprintf(buf, size, "%s", values.address); break;
    case KEY_APP:             snprintf(buf, size, "%s", values.app); break;
    case KEY_WIDTH:           snprintf(buf, size, "%d", values.width); break;
    case KEY_HEIGHT:          snprintf(buf, size, "%d", values.height); break;
    case KEY_FPS:             snprintf(buf, size, "%d", values.fps); break;
    case KEY_BITRATE:         snprintf(buf, size, "%d", values.bitrate > 0 ? values.bitrate : -1); break;
    case KEY_REMOTE:          snprintf(buf, size, "%s", values.remote == STREAM_CFG_LOCAL ? "no" :
                                                        values.remote == STREAM_CFG_REMOTE ? "yes" : "auto"); break;
    case KEY_MOUSE_MODE:      snprintf(buf, size, "%s", mouse_values[wrap(values.mouse_mode, 3)]); break;
    case KEY_DISABLE_GAMEPAD: snprintf(buf, size, "%s", values.disable_gamepad ? "true" : "false"); break;
    case KEY_SWAP_BUTTONS:    snprintf(buf, size, "%s", values.swap_buttons ? "true" : "false"); break;
    case KEY_AUTOSTREAM:      snprintf(buf, size, "%s", values.autostream ? "true" : "false"); break;
    case KEY_QUITAPPAFTER:    snprintf(buf, size, "%s", values.quitappafter ? "true" : "false"); break;
    case KEY_LOCALAUDIO:      snprintf(buf, size, "%s", values.localaudio ? "true" : "false"); break;
    default:                  buf[0] = '\0'; break;
  }
}

static bool should_write_key(int key)
{
  // an empty address would be parsed as garbage, leave it commented out
  return !(key == KEY_ADDRESS && values.address[0] == '\0');
}

// Updates the managed options in the config file in place, keeping all
// comments and any options this menu doesn't know about.
static bool save_config_file(const char* path)
{
  char* old = NULL;
  long old_len = 0;

  FILE* f = fopen(path, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    old_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (old_len > 0) {
      old = malloc(old_len + 1);
      if (!old) {
        fclose(f);
        return false;
      }
      old_len = fread(old, 1, old_len, f);
      old[old_len] = '\0';
    }
    fclose(f);
  }

  f = fopen(path, "wb");
  if (!f) {
    free(old);
    return false;
  }

  bool written[KEY_COUNT] = { false };
  char value[300];

  long pos = 0;
  while (pos < old_len) {
    const char* line = old + pos;
    const char* nl = memchr(line, '\n', old_len - pos);
    size_t len = nl ? (size_t) (nl - line) : (size_t) (old_len - pos);
    size_t content_len = len;
    if (content_len > 0 && line[content_len - 1] == '\r')
      content_len--;

    bool commented;
    int key = match_config_line(line, content_len, &commented);
    if (key >= 0 && !written[key] && should_write_key(key)) {
      format_config_value(key, value, sizeof(value));
      fprintf(f, "%s = %s\n", config_keys[key], value);
      written[key] = true;
    } else if (key >= 0 && !commented) {
      // a duplicate (or now unset) option would override the value above
      fputc('#', f);
      fwrite(line, 1, content_len, f);
      fputc('\n', f);
    } else {
      fwrite(line, 1, content_len, f);
      fputc('\n', f);
    }

    pos += len + (nl ? 1 : 0);
  }

  for (int k = 0; k < KEY_COUNT; k++) {
    if (!written[k] && should_write_key(k)) {
      format_config_value(k, value, sizeof(value));
      fprintf(f, "%s = %s\n", config_keys[k], value);
    }
  }

  free(old);
  bool ok = ferror(f) == 0;
  if (fclose(f) != 0)
    ok = false;
  return ok;
}

static void apply_to_config(PCONFIGURATION config)
{
  if (values.address[0])
    config->address = strdup(values.address);
  else
    config->address = NULL;

  config->app = strdup(values.app);
  config->stream.width = values.width;
  config->stream.height = values.height;
  config->stream.fps = values.fps;
  config->stream.bitrate = values.bitrate > 0 ? values.bitrate : -1;
  config->stream.streamingRemotely = values.remote;
  config->quitappafter = values.quitappafter;
  config->localaudio = values.localaudio;

  mouse_mode = (mouse_modes) values.mouse_mode;
  disable_gamepad = values.disable_gamepad;
  swap_buttons = values.swap_buttons;
  autostream = values.autostream;
}

static int save_and_apply(PCONFIGURATION config)
{
  if (!save_config_file(MOONLIGHT_WIIU_CONFIG_FILE)) {
    snprintf(message_buffer, sizeof(message_buffer),
             "Failed to save settings to the SD card.\nThe changes were applied for this session only.\n");
    is_error = 1;
  } else {
    char host_config_file[PATH_MAX];
    snprintf(host_config_file, sizeof(host_config_file), MOONLIGHT_WIIU_PATH "/hosts/%s.conf", values.address);
    if (values.address[0] && access(host_config_file, R_OK) != -1) {
      snprintf(message_buffer, sizeof(message_buffer),
               "Settings saved.\nNote: hosts/%s.conf overrides some settings on launch.\n", values.address);
    } else {
      snprintf(message_buffer, sizeof(message_buffer), "Settings saved.\n");
    }
    is_error = 0;
  }

  apply_to_config(config);
  free_apps();
  return SETTINGS_SAVED;
}

int settings_update(PCONFIGURATION config)
{
  uint32_t triggered = wiiu_input_buttons_triggered();
  uint32_t btns = apply_repeat(triggered, wiiu_input_buttons_held());

  if (editing_address) {
    int* octet = &address_octets[address_cursor];
    if (btns & VPAD_BUTTON_UP)
      *octet = wrap(*octet + 1, 256);
    if (btns & VPAD_BUTTON_DOWN)
      *octet = wrap(*octet - 1, 256);
    if (btns & VPAD_BUTTON_R)
      *octet = wrap(*octet + 10, 256);
    if (btns & VPAD_BUTTON_L)
      *octet = wrap(*octet - 10, 256);
    if (btns & VPAD_BUTTON_LEFT)
      address_cursor = wrap(address_cursor - 1, 4);
    if (btns & VPAD_BUTTON_RIGHT)
      address_cursor = wrap(address_cursor + 1, 4);
    if (btns & (VPAD_BUTTON_A | VPAD_BUTTON_B))
      end_address_edit();

    draw_menu();
    return SETTINGS_ACTIVE;
  }

  if (btns & VPAD_BUTTON_UP)
    selected = wrap(selected - 1, ITEM_COUNT);
  if (btns & VPAD_BUTTON_DOWN)
    selected = wrap(selected + 1, ITEM_COUNT);
  if (btns & VPAD_BUTTON_LEFT)
    change_value(selected, -1, false);
  if (btns & VPAD_BUTTON_RIGHT)
    change_value(selected, 1, false);
  if (selected == ITEM_BITRATE) {
    if (btns & VPAD_BUTTON_L)
      change_value(selected, -1, true);
    if (btns & VPAD_BUTTON_R)
      change_value(selected, 1, true);
  }

  if (btns & VPAD_BUTTON_A) {
    switch (selected) {
      case ITEM_ADDRESS:
        begin_address_edit();
        break;
      case ITEM_SAVE:
        return save_and_apply(config);
      case ITEM_CANCEL:
        free_apps();
        return SETTINGS_CANCELLED;
      default:
        change_value(selected, 1, false);
        break;
    }
  }

  if (btns & VPAD_BUTTON_PLUS)
    return save_and_apply(config);

  if (btns & VPAD_BUTTON_B) {
    free_apps();
    return SETTINGS_CANCELLED;
  }

  draw_menu();
  return SETTINGS_ACTIVE;
}
