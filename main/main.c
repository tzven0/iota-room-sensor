// IOTA CClient Example

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "cclient/iota_client_extended_api.h"
#include "cclient/iota_client_core_api.h"
#include "cclient/types/types.h"
#include <inttypes.h>

static const char *TAG = "iota_main";

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int CONNECTED_BIT = BIT0;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  switch (event->event_id)
  {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void wifi_conn_init(void)
{
  tcpip_adapter_init();
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {
      .sta = {
          .ssid = CONFIG_WIFI_SSID,
          .password = CONFIG_WIFI_PASSWORD,
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

static void iota_client_task(void *p)
{

  while (1)
  {
    /* Wait for the callback to set the CONNECTED_BIT in the event group. */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
    // test get_node_info
    trit_t trytes_out[NUM_TRYTES_HASH + 1];
    size_t trits_count = 0;
    retcode_t ret = RC_OK;
    iota_client_service_t client_service;
    get_node_info_res_t *node_res = get_node_info_res_new();
    client_service.http.path = "/";
    client_service.http.content_type = "application/json";
    client_service.http.accept = "application/json";
    client_service.http.host = CONFIG_IRI_NODE_URI;
    client_service.http.port = atoi(CONFIG_IRI_NODE_PORT);
    client_service.http.api_version = 1;
    client_service.serializer_type = SR_JSON;
    logger_init();
    logger_output_register(stdout);
    logger_output_level_set(stdout, LOGGER_DEBUG);
    iota_client_core_init(&client_service);
    iota_client_extended_init();

    ret = iota_client_get_node_info(&client_service, node_res);
    if (ret == RC_OK)
    {
      printf("appName %s \n", node_res->app_name->data);
      printf("appVersion %s \n", node_res->app_version->data);
      trits_count = flex_trits_to_trytes(trytes_out, NUM_TRYTES_HASH,
                                         node_res->latest_milestone, NUM_TRITS_HASH,
                                         NUM_TRITS_HASH);
      if (trits_count == 0)
      {
        printf("trit converting failed\n");
        goto done;
      }
      trytes_out[NUM_TRYTES_HASH] = '\0';
      printf("latestMilestone %s \n", trytes_out);
      printf("latestMilestoneIndex %zu\n", (uint32_t)node_res->latest_milestone_index);
      trits_count = flex_trits_to_trytes(trytes_out, NUM_TRYTES_HASH,
                                         node_res->latest_milestone, NUM_TRITS_HASH,
                                         NUM_TRITS_HASH);
      if (trits_count == 0)
      {
        printf("trit converting failed\n");
        goto done;
      }
      trytes_out[NUM_TRYTES_HASH] = '\0';
      printf("latestSolidSubtangleMilestone %s \n", trytes_out);

      printf("latestSolidSubtangleMilestoneIndex %zu\n",
             node_res->latest_solid_subtangle_milestone_index);
      printf("milestoneStratIndex %zu\n", node_res->milestone_start_index);
      printf("neighbors %d \n", node_res->neighbors);
      printf("packetsQueueSize %d \n", node_res->packets_queue_size);
      printf("time %" PRIu64 "\n", node_res->time);
      printf("tips %zu \n", node_res->tips);
      printf("transactionsToRequest %zu\n", node_res->transactions_to_request);
    }else{
      ESP_LOGI(TAG, "get_node_info error:%s", error_2_string(ret));
    }

  done:
    get_node_info_res_free(&node_res);
    // end of get_node_info

    for (int i = 30; i >= 0; i--)
    {
      ESP_LOGI(TAG, "Restarting in %d seconds...", i);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
  }
}
void app_main()
{
  ESP_LOGI(TAG, "app_main starting...");

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

  ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  ESP_ERROR_CHECK(nvs_flash_init());
  wifi_conn_init();
  xTaskCreate(iota_client_task, "iota_client", 20480, NULL, 5, NULL);
}
