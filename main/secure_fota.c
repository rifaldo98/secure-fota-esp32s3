#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "soc/usb_serial_jtag_reg.h"
#include "hal/usb_serial_jtag_ll.h"
#include "soc/io_mux_reg.h"
#include "hal/gpio_hal.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"
/* ================= CONFIG ================= */

#define LED_TEST 2    // test LED
#define RGB_PIN 48
#define RGB_LED_NUM 1
#define BOOT_BTN_GPIO 10


#define WIFI_SSID "rumahkuning"
#define WIFI_PASS "indonesia17"

static led_strip_handle_t rgb_strip;
static const char *TAG = "FOTA";
typedef enum {
    LED_NORMAL,
    LED_OTA,
    LED_RECOVERY
} led_state_t;
static volatile led_state_t current_led_state = LED_NORMAL;

/* ================= WIFI STA ================= */
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void url_decode(char *src)
{
    char *dst = src;
    char hex[3] = {0};

    while (*src) {
        if (*src == '%') {
            hex[0] = src[1];
            hex[1] = src[2];
            *dst = (char) strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
    }
    *dst = 0;
}

void rgb_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_PIN,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 20 * 1000 * 1000, // 20 MHz lebih stabil
        .mem_block_symbols = 64,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &rgb_strip));
}

void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(rgb_strip, 0, r, g, b);
    led_strip_refresh(rgb_strip);
}

/* ================= LED STATE ================= */

void disable_usb_jtag(void)
{
    // Matikan USB Serial/JTAG
    REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);

    // Paksa GPIO48 ke fungsi GPIO Matrix
    PIN_FUNC_SELECT(IO_MUX_GPIO48_REG, PIN_FUNC_GPIO);

    // Reset function override
    gpio_reset_pin(48);
}
/* ================= LED STATUS TASK ================= */
void led_status_task(void *pv)
{

    ESP_LOGW(TAG, "Testing GPIO48...");
    gpio_set_level(RGB_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(RGB_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        led_state_t s = current_led_state;
		if (s == LED_NORMAL) {
			rgb_set(0, 0, 255);   // biru
			vTaskDelay(pdMS_TO_TICKS(500));
			rgb_set(0, 0, 0);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		
		else if (s == LED_OTA) {
			rgb_set(255, 0, 0);  // merah
			vTaskDelay(pdMS_TO_TICKS(150));
			rgb_set(0, 0, 0);
			vTaskDelay(pdMS_TO_TICKS(150));
		}
		
		else if (s == LED_RECOVERY) {
			for(int i=0;i<2;i++){
				rgb_set(0, 255, 0); // hijau
				vTaskDelay(pdMS_TO_TICKS(120));
				rgb_set(0, 0, 0);
				vTaskDelay(pdMS_TO_TICKS(120));
			}
			vTaskDelay(pdMS_TO_TICKS(800));
		}
    }
}

/* ================= LED TEST ================= */
void led_test_task(void *pv)
{
    gpio_reset_pin(LED_TEST);
    gpio_set_direction(LED_TEST, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_TEST, 1);
        vTaskDelay(pdMS_TO_TICKS(300));
        gpio_set_level(LED_TEST, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
		ESP_LOGW(TAG, "led nyala uhuy");
		
		//*((volatile int *)0) = 0;  // 2x panic
    }
}

/* ================= BUTTON ================= */
bool boot_button_pressed(void)
{
    gpio_reset_pin(BOOT_BTN_GPIO);
    gpio_set_direction(BOOT_BTN_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_BTN_GPIO, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50));
    return gpio_get_level(BOOT_BTN_GPIO) == 0;
}

/* ================= WIFI ================= */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGW(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void load_wifi_from_nvs(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t len1=32, len2=64;

    if (nvs_open("wifi", NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_str(nvs, "ssid", ssid, &len1) == ESP_OK &&
            nvs_get_str(nvs, "pass", pass, &len2) == ESP_OK) {
            ESP_LOGW(TAG, "Loaded WiFi from NVS: %s", ssid);
        }
        nvs_close(nvs);
    }
}

void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    char ssid[32] = WIFI_SSID;
	char pass[64] = WIFI_PASS;
	load_wifi_from_nvs(ssid, pass);

	wifi_config_t wifi_config = {0};
	strcpy((char*)wifi_config.sta.ssid, ssid);
	strcpy((char*)wifi_config.sta.password, pass);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, pdMS_TO_TICKS(5000));
}

/* ================= FAILSAFE ================= */
void ota_stability_task(void *pv)
{
    ESP_LOGW(TAG, "Waiting 10s before VALID...");
    vTaskDelay(pdMS_TO_TICKS(10000));

    ESP_LOGW(TAG, "MARKING OTA IMAGE AS VALID");
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();

    if (err == ESP_OK) {
        ESP_LOGW(TAG, "OTA VALIDATED & LOCKED");
    } else {
        ESP_LOGE(TAG, "FAILED TO MARK VALID: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}


/* ================= OTA ================= */
void ota_task_from_url(void *pv)
{
    current_led_state = LED_OTA;
    char *url = (char *)pv;

    esp_http_client_config_t http_cfg = { .url=url, .timeout_ms=30000 };
    esp_https_ota_handle_t handle = NULL;
    esp_https_ota_config_t ota_cfg = { .http_config=&http_cfg };

    esp_https_ota_begin(&ota_cfg, &handle);

    while (1) {
        esp_err_t ret = esp_https_ota_perform(handle);
        if (ret == ESP_ERR_HTTPS_OTA_IN_PROGRESS) continue;
        if (ret == ESP_OK) break;
        esp_https_ota_abort(handle);
        vTaskDelete(NULL);
    }

    esp_https_ota_finish(handle);
    esp_restart();
}

/* ================= RECOVERY AP ================= */
void wifi_init_apsta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid="ESP32-RECOVERY",
            .password="12345678",
            .authmode=WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection=2
        }
    };

    char ssid[32] = WIFI_SSID;
    char pass[64] = WIFI_PASS;
    load_wifi_from_nvs(ssid, pass);

    wifi_config_t sta_cfg = {0};
    strcpy((char*)sta_cfg.sta.ssid, ssid);
    strcpy((char*)sta_cfg.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();

    esp_wifi_connect();
}

/* ================= HTTP ================= */
static esp_err_t root_get(httpd_req_t *req)
{
    const char html[] =
        "<h2>ESP32 Recovery</h2>"
        "<form action='/save' method='POST'>SSID:<input name='ssid'><br>"
        "PASS:<input name='pass'><br><input type='submit'></form><br>"
        "<form action='/ota' method='POST'>OTA URL:<input name='url'><br>"
        "<input type='submit'></form>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    buf[len] = 0;

    ESP_LOGW(TAG, "RAW POST: %s", buf);

    char ssid[32], pass[64];
    if (sscanf(buf, "ssid=%31[^&]&pass=%63s", ssid, pass) != 2) {
        ESP_LOGE(TAG, "Parse WiFi failed");
        httpd_resp_sendstr(req, "Parse failed");
        return ESP_FAIL;
    }

    url_decode(ssid);
    url_decode(pass);

    ESP_LOGW(TAG, "WiFi saved: SSID='%s' PASS='%s'", ssid, pass);

    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "pass", pass));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    httpd_resp_sendstr(req, "WiFi saved OK");
    return ESP_OK;
}

static esp_err_t ota_post(httpd_req_t *req)
{
    char buf[256]; int len = httpd_req_recv(req, buf, sizeof(buf)-1); buf[len]=0;
    char url[200];
	sscanf(buf,"url=%199s",url);
	url_decode(url);
    xTaskCreate(ota_task_from_url,"ota",8192,strdup(url),5,NULL);
    httpd_resp_sendstr(req,"OTA started");
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get,
    .user_ctx = NULL
};

httpd_uri_t save = {
    .uri = "/save",
    .method = HTTP_POST,
    .handler = save_post,
    .user_ctx = NULL
};

httpd_uri_t ota = {
    .uri = "/ota",
    .method = HTTP_POST,
    .handler = ota_post,
    .user_ctx = NULL
};


    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &ota);
}

/* ================= MODES ================= */
void start_recovery_mode(void)
{
    current_led_state = LED_RECOVERY;
    wifi_init_apsta();
    start_webserver();
    while(1) vTaskDelay(portMAX_DELAY);
}

void app_logic(void)
{
    current_led_state = LED_NORMAL;

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    esp_ota_get_state_partition(running,&state);

    if(state==ESP_OTA_IMG_PENDING_VERIFY)
        xTaskCreate(ota_stability_task,"stable",4096,NULL,5,NULL);

    xTaskCreate(led_test_task,"test",2048,NULL,1,NULL);

    while(1) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ================= MAIN ================= */
void app_main(void)
{
    disable_usb_jtag();
	rgb_init();
	
	nvs_flash_init();
    xTaskCreate(led_status_task,"led_status",2048,NULL,1,NULL);

    if (boot_button_pressed()) {
        start_recovery_mode();
    } else {
        wifi_init_sta();
        app_logic();
    }
}


