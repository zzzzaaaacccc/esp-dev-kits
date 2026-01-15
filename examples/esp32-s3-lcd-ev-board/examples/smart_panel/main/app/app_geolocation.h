#ifndef APP_GEOLOCATION_H
#define APP_GEOLOCATION_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char region[64];  
    char city[64];
    char country[64];
    char timezone[32];
    float latitude;
    float longitude;
} geolocation_t;


bool app_geolocation_fetch(geolocation_t *location);
geolocation_t* app_geolocation_get(void);

#endif // APP_GEOLOCATION_H
