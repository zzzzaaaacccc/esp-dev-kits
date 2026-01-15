#include "app_geolocation.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "geolocation";

static geolocation_t g_location = {0};
static bool g_location_fetched = false;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer = NULL;
    static int output_len = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (output_buffer == NULL) {
                    output_buffer = (char *) malloc(evt->data_len + 1);
                    output_len = 0;
                } else {
                    char *tmp = realloc(output_buffer, output_len + evt->data_len + 1);
                    if (tmp == NULL) {
                        ESP_LOGE(TAG, "Failed to reallocate memory");
                        free(output_buffer);
                        return ESP_FAIL;
                    }
                    output_buffer = tmp;
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
                output_buffer[output_len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (output_buffer != NULL) {
                ESP_LOGI(TAG, "API Response: %s", output_buffer);
    
                cJSON *root = cJSON_Parse(output_buffer);
                if (root) {
                    cJSON *region = cJSON_GetObjectItem(root, "regionName");
                    cJSON *city = cJSON_GetObjectItem(root, "city");
                    cJSON *country = cJSON_GetObjectItem(root, "country");
                    cJSON *timezone = cJSON_GetObjectItem(root, "timezone");
                    cJSON *lat = cJSON_GetObjectItem(root, "lat");
                    cJSON *lon = cJSON_GetObjectItem(root, "lon");

                    if (region && region->valuestring && strlen(region->valuestring) > 0) {
                        strncpy(g_location.region, region->valuestring, sizeof(g_location.region) - 1);
                        ESP_LOGI(TAG, "Region found: %s", g_location.region);
                    } else {
                        ESP_LOGI(TAG, "Region not found or empty");
                    }
                    if (city && city->valuestring) {
                        strncpy(g_location.city, city->valuestring, sizeof(g_location.city) - 1);
                    }
                    if (country && country->valuestring) {
                        strncpy(g_location.country, country->valuestring, sizeof(g_location.country) - 1);
                    }
                    if (timezone && timezone->valuestring) {
                        strncpy(g_location.timezone, timezone->valuestring, sizeof(g_location.timezone) - 1);
                    }
                    if (lat && lat->valuedouble) {
                        g_location.latitude = lat->valuedouble;
                    }
                    if (lon && lon->valuedouble) {
                        g_location.longitude = lon->valuedouble;
                    }

                    cJSON_Delete(root);
                    ESP_LOGI(TAG, "Location fetched: region='%s', city='%s', country='%s'", g_location.region, g_location.city, g_location.country);
                    g_location_fetched = true;
                } else {
                    ESP_LOGE(TAG, "Failed to parse JSON response");
                }
                free(output_buffer);
                output_buffer = NULL;
                output_len = 0;
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
                output_len = 0;
            }
            break;
        case HTTP_EVENT_ERROR:
        case HTTP_EVENT_ON_CONNECTED:
        case HTTP_EVENT_HEADERS_SENT:
        case HTTP_EVENT_ON_HEADER:
        case HTTP_EVENT_REDIRECT:
            break;
        default:
            break;
    }
    return ESP_OK;
}

bool app_geolocation_fetch(geolocation_t *location)
{
    if (location == NULL) {
        return false;
    }

    ESP_LOGI(TAG, "Fetching geolocation from ip-api.com...");

    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/",
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK) {
        memcpy(location, &g_location, sizeof(geolocation_t));
        return g_location_fetched;
    }

    ESP_LOGE(TAG, "Geolocation fetch failed: %s", esp_err_to_name(err));
    return false;
}

geolocation_t* app_geolocation_get(void)
{
    return &g_location;
}
