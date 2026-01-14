#ifndef APP_GEOLOCATION_H
#define APP_GEOLOCATION_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char region[64];    // regionName (e.g., "Central Singapore")
    char city[64];
    char country[64];
    char timezone[32];
    float latitude;
    float longitude;
} geolocation_t;

/**
 * @brief Fetch geolocation from ip-api.com
 * 
 * @param location Pointer to geolocation_t struct to fill with data
 * @return true if successful, false otherwise
 */
bool app_geolocation_fetch(geolocation_t *location);

/**
 * @brief Get the last fetched geolocation
 * 
 * @return Pointer to geolocation_t struct with cached data
 */
geolocation_t* app_geolocation_get(void);

#endif // APP_GEOLOCATION_H
