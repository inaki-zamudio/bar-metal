#include <stdio.h>

#include "esp_log.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio.h"
#include "buttons.h"
#include "display.h"
#include "esp_timer.h"
#include "peer.h"

static const char *TAG = "demo";
uint8_t master_mac[6] = {0x80, 0x45, 0x6B, 0x36, 0x00, 0xc8};

#define UI_PERIOD_MS 50

static const int s_button_freq[BTN_COUNT] = {
    [BTN_UP] = 659,    [BTN_DOWN] = 440,   [BTN_LEFT] = 494,
    [BTN_RIGHT] = 587, [BTN_CENTER] = 784, [BTN_KEY1] = 262,
    [BTN_KEY2] = 330,  [BTN_KEY3] = 392,   [BTN_KEY4] = 523,
};

void i64_to_bytes_be(int64_t value, uint8_t out[8]) {
  uint64_t v = (uint64_t)
      value; // cast a unsigned para que los shifts estén bien definidos
  for (int i = 7; i >= 0; i--) {
    out[i] = (uint8_t)(v & 0xFF);
    v >>= 8;
  }
}

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

  display_text(0, 29, "EMISOR", false);
  display_text(0, 43, mac, false);
}
void on_data_sent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  printf("Send Status: %s\n",
         status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}
void on_data_recv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data,
                  int data_len) {
  esp_now_send(esp_now_info->src_addr, (const uint8_t *)("pong"), 4);
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
  peer_add_mac(master_mac);
  uint8_t buf[8];
  esp_now_register_recv_cb(on_data_recv);
  esp_now_register_send_cb(on_data_sent);
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
    printf("mic level %d\n", mic_level);
    if (display_ok) {
      char mac_str[18];
      peer_get_mac(mac_str);
      draw_ui(mac_str, mic_level, audio_ok);
      display_flush();
    }

    if (audio_ok && ++log_div >= 1000 / UI_PERIOD_MS) {
      log_div = 0;
      ESP_LOGI(TAG, "mic level: %d", mic_level);
    }
    if (audio_ok) {
      if (mic_level > 65) {
        int64_t time = esp_timer_get_time();
        i64_to_bytes_be(time, buf);
        esp_now_send(master_mac, buf, 8);
      }
    }
  }
}
