idf_component_register(
    SRC_DIRS
        "."
        "app"
        "ui"
        "icon"
        "lv_fonts"
    INCLUDE_DIRS
    "."
        "app"
        "ui"
        "lv_fonts")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
