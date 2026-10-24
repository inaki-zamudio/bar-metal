#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio.h"
#include "buttons.h"
#include "display.h"
#include "esp_timer.h"
#include "peer.h"
#include <string.h>

static const char *TAG = "demo";
uint8_t mac_slaves[2][6] = {{0x80, 0x45, 0x6B, 0x36, 0x00, 0xf4},
                            {0x80, 0x45, 0x6B, 0x36, 0x01, 0x10}};

uint8_t deltas[2] = {0, 0};

int64_t time_emisor[2] = {0, 0};

uint8_t emisor_sync[2] = {0, 0};

#define UI_PERIOD_MS 50

typedef struct {
  uint8_t d1;
  uint8_t d2;
} tuple;

tuple triangular(uint8_t d1, uint8_t d2) { // d1 = t3-t1 d2 = t2-t1
  float c = 343.0;
  float d21 = c * d1;
  float d31 = c * d2;
  tuple res;
  // 0,0
  // 1,0 = x2,0
  // 1,1 = x3,y3
  float g = (1 - d21 * d21); //(x2*x2 - d21*d21)/x2

  float h = -d21 //-d21/x2

            float d = (2.0 - d31 * d31) / 1.0; //(x3*x3 + y3*y3 - d31*d31)/y3

  float e = -d31 //-d31/y3

            float f = -1 //
                         //
                      float i = d / 2.0 + f * g / 2.0;
  float j = e + f * h;

  float A = h * h + j * j - 1;
  float B = (g * h) + (2 * i * j);
  float C = g * g / 4.0 + i * i;

  float R1_1 = (-B - (B * B - 4 * A * C)) / (2 * A);
  float R1_2 = (-B + (B * B - 4 * A * C)) / (2 * A);

  float R1 = 0;

  if (R1_1 > 0) {
    R1 = R1_1
  } else {
    R1 = R1_2;
  }

  float X = g / 2.0 + h * R1;
  float Y = i + j * R1;

  res.d1 = (uint8_t)X;
  res.d2 = (uint8_t)Y;
  return res;
}

int64_t bytes_to_i64_be(const uint8_t *data, int data_len) {
  if (data_len <= 0 || data_len > 8)
    return 0; // o manejá el error como prefieras

  uint64_t v = 0;
  for (int i = 0; i < data_len; i++)
    v = (v << 8) | data[i];

  // extensión de signo si hay menos de 8 bytes
  if (data_len < 8 && (data[0] & 0x80))
    v |= ~0ULL << (data_len * 8);

  return (int64_t)v;
}

int compare_mac(uint8_t *mac1, uint8_t *mac2) {
  if (mac1[0] == mac2[0] && mac1[1] == mac2[1] && mac1[2] == mac2[2] &&
      mac1[3] == mac2[3] && mac1[4] == mac2[4] && mac1[5] == mac2[5]) {
    return 1;
  }
  return 0;
}

static const int s_button_freq[BTN_COUNT] = {
    [BTN_UP] = 659,    [BTN_DOWN] = 440,   [BTN_LEFT] = 494,
    [BTN_RIGHT] = 587, [BTN_CENTER] = 784, [BTN_KEY1] = 262,
    [BTN_KEY2] = 330,  [BTN_KEY3] = 392,   [BTN_KEY4] = 523,
};

static void draw_key(int x, int y, int size, bool pressed) {
  if (pressed) {
    display_fill_rect(x, y, size, size, true);
  } else {
    display_fill_rect(x, y, size, size, false);
    display_draw_rect(x, y, size, size);
  }
}

static void draw_ui(char mac[18], int mic_level, bool audio_ok) {
  char line[24];

  display_clear();
  display_text(2, 1, "HACKATHON BOARD", true);

  display_text(0, 14, "MIC", false);
  display_draw_rect(22, 13, 82, 10);
  display_fill_rect(24, 15, mic_level * 78 / 100, 6, true);
  if (audio_ok) {
    snprintf(line, sizeof(line), "%3d", mic_level);
    display_text(107, 14, line, false);
  } else {
    display_text(107, 14, "ERR", false);
  }

  display_text(0, 29, "RECEPTOR", false);
  display_text(0, 43, mac, false);
}
int str_data_compare(const char *cmp, const char *cmp1, int len) {
  int res = len;
  int len1 = strlen(cmp1);
  if (len1 < len) {
    return len - len1;
  }
  for (int i = 0; i < len; i++) {
    if (cmp[i] == cmp1[i])
      res--;
  }
  return res;
}
void on_data_sent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  printf("Send Status: %s\n",
         status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}
void on_data_recv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data,
                  int data_len) {
  for (int i = 0; i < 2; i++) {
    if (compare_mac(esp_now_info->src_addr, mac_slaves[i])) {
      if (str_data_compare((const char *)data, "pong", data_len) == 0) {
        // sincronizar
        int64_t time = esp_timer_get_time();
        deltas[i] = time - deltas[i];
        emisor_sync[i] = 1;
        printf("Me sincronice mi delta es %d\n", deltas[i]);
      } else {
        int64_t ti = esp_timer_get_time();
        time_emisor[i] = ti - deltas[i];
        printf("ruido en %d en t = %lld con delta d = %d\n tf = %lld", i, ti,
               deltas[i], time_emisor[i]);
      }
    }
  }
}

void app_main(void) {
  ESP_LOGI(TAG, "Radar de sonido");

  bool display_ok = display_init() == ESP_OK;
  if (!display_ok) {
    ESP_LOGE(TAG, "display init failed - continuing without screen");
  }

  ESP_ERROR_CHECK(buttons_init());

  if (display_ok) {
    display_clear();
    display_text(10, 8, "Hello, Hackathon!", false);
    display_text(10, 24, "ESP32-S3 dev board", false);
    display_text(10, 40, "Self test...", false);
    display_flush();
  }

  bool audio_ok = audio_init() == ESP_OK;
  if (audio_ok) {
    const int notes[] = {523, 659, 784};
    for (int i = 0; i < 3; i++) {
      audio_play_tone(notes[i], 120);
    }
  } else {
    ESP_LOGE(TAG, "audio init failed - continuing without sound");
  }
  vTaskDelay(pdMS_TO_TICKS(500));

  QueueHandle_t events = buttons_event_queue();
  int log_div = 0;
  peer_init();
  peer_add_mac(mac_slaves[0]);
  peer_add_mac(mac_slaves[1]);
  char msg_buf[50];
  esp_now_register_recv_cb(on_data_recv);
  esp_now_register_send_cb(on_data_sent);
  int time = esp_timer_get_time();
  deltas[0] = time;
  deltas[1] = time;
  esp_now_send(mac_slaves[0], ((const uint8_t *)"ping"), 4);
  esp_now_send(mac_slaves[1], ((const uint8_t *)"ping"), 4);
  while (1) {
    button_event_t ev;
    if (xQueueReceive(events, &ev, pdMS_TO_TICKS(UI_PERIOD_MS)) == pdTRUE) {
      ESP_LOGI(TAG, "button %s %s", buttons_name(ev.id),
               ev.pressed ? "pressed" : "released");
      if (ev.pressed && audio_ok) {
        audio_play_tone(s_button_freq[ev.id], 100);
      }
    }

    int mic_level = audio_ok ? audio_mic_level() : 0;
    if (display_ok) {
      char mac_str[18];
      peer_get_mac(mac_str);
      draw_ui(mac_str, mic_level, audio_ok);
      display_flush();
    }

    if (audio_ok && ++log_div >= 1000 / UI_PERIOD_MS) {
      log_div = 0;
      if (emisor_sync[0] > 0 && emisor_sync[1] > 0) {
        int64_t time = esp_timer_get_time();
        if (mic_level > 65) {
          printf("tiempos de escucha tM %lld t0 %lld t1 %lld", time,
                 time_emisor[0], time_emisor[1]);
        }
        if (abs((int)time_emisor[0] - (int)time_emisor[1]) < 1000) {
          printf("RUIDO tM %lld t0 %lld t1 %lld", time, time_emisor[0],
                 time_emisor[1]);
        }
      }
    }
  }
}
