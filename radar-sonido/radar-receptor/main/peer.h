#pragma once
#include "esp_check.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
void peer_init(void);
void peer_get_mac(char mac[18]);
void peer_add_mac(uint8_t mac[6]);
