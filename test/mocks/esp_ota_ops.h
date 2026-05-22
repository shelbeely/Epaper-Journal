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

inline esp_partition_t esp_ota_mock_running_partition{0};
inline bool esp_ota_mock_has_running_partition = false;
inline esp_ota_img_states_t esp_ota_mock_running_state = ESP_OTA_IMG_UNDEFINED;
inline bool esp_ota_mock_mark_valid_called = false;
inline bool esp_ota_mock_mark_invalid_called = false;

inline void esp_ota_mock_reset() {
    esp_ota_mock_has_running_partition = false;
    esp_ota_mock_running_state = ESP_OTA_IMG_UNDEFINED;
    esp_ota_mock_mark_valid_called = false;
    esp_ota_mock_mark_invalid_called = false;
}

inline void esp_ota_mock_set_running_partition_state(esp_ota_img_states_t state) {
    esp_ota_mock_has_running_partition = true;
    esp_ota_mock_running_state = state;
}

inline const esp_partition_t* esp_ota_get_running_partition() {
    return esp_ota_mock_has_running_partition ? &esp_ota_mock_running_partition : nullptr;
}
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t* /*part*/,
                                             esp_ota_img_states_t* state) {
    *state = esp_ota_mock_running_state;
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
inline void esp_ota_mark_app_valid_cancel_rollback() {
    esp_ota_mock_mark_valid_called = true;
    esp_ota_mock_running_state = ESP_OTA_IMG_VALID;
}
inline void esp_ota_mark_app_invalid_rollback_and_reboot() {
    esp_ota_mock_mark_invalid_called = true;
}

// esp_restart() is in esp_system.h on ESP-IDF but referenced directly here
inline void esp_restart() {}
