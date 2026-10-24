#include "peer.h"
void peer_get_mac(char mac[18]) {
  uint8_t my_mac[6];
  esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
  snprintf(mac, 18, "%02X:%02X:%02X:%02x:%02X:%02X", my_mac[0], my_mac[1],
           my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
}
void peer_init(void) {
  ESP_ERROR_CHECK(nvs_flash_init());

  // Initialize Wi-Fi in Station Mode (but do not connect to a router)
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  uint8_t my_mac[6];
  esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
  printf("\n>>> MY MAC ADDRESS: {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
         "0x%02X} <<<\n\n",
         my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);

  ESP_ERROR_CHECK(esp_now_init());
}

void peer_add_mac(uint8_t mac[6]) {

  esp_now_peer_info_t peer_info = {};
  memcpy(peer_info.peer_addr, mac, 6);
  peer_info.channel = 0;     // 0 means use the current Wi-Fi channel
  peer_info.encrypt = false; // Disable encryption for faster speed right now
  ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}
