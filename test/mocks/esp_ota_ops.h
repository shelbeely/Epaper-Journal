#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/esp_ota_ops.h — stub for ESP-IDF OTA partition management APIs
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <cstdint>

typedef int      esp_err_t;
typedef uint32_t esp_ota_handle_t;

// Opaque partition descriptor
struct esp_partition_t { int slot; };

#define ESP_OK    0
#define ESP_FAIL -1

#define OTA_SIZE_UNKNOWN ((size_t)-1)

typedef enum {
    ESP_OTA_IMG_UNDEFINED,
    ESP_OTA_IMG_NEW,
    ESP_OTA_IMG_PENDING_VERIFY,
    ESP_OTA_IMG_VALID,
    ESP_OTA_IMG_INVALID,
    ESP_OTA_IMG_ABORTED,
} esp_ota_img_states_t;

inline const esp_partition_t* esp_ota_get_running_partition() {
    return nullptr;
}
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t* /*part*/,
                                             esp_ota_img_states_t* state) {
    *state = ESP_OTA_IMG_UNDEFINED;
    return ESP_OK;
}
inline const esp_partition_t* esp_ota_get_next_update_partition(
        const esp_partition_t* /*start*/) {
    return nullptr;
}
inline esp_err_t esp_ota_begin(const esp_partition_t* /*part*/,
                               size_t /*image_size*/,
                               esp_ota_handle_t* /*out*/) {
    return ESP_OK;
}
inline esp_err_t esp_ota_write(esp_ota_handle_t /*handle*/,
                               const void* /*data*/, size_t /*size*/) {
    return ESP_OK;
}
inline esp_err_t esp_ota_abort(esp_ota_handle_t /*handle*/) { return ESP_OK; }
inline esp_err_t esp_ota_end(esp_ota_handle_t /*handle*/)   { return ESP_OK; }
inline esp_err_t esp_ota_set_boot_partition(
        const esp_partition_t* /*part*/) {
    return ESP_OK;
}
inline void esp_ota_mark_app_valid_cancel_rollback() {}
inline void esp_ota_mark_app_invalid_rollback_and_reboot() {}

// esp_restart() is in esp_system.h on ESP-IDF but referenced directly here
inline void esp_restart() {}
