/*
 * Copyright (c) 2026 Your Name
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_encoder_mouse_scroll

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>

#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

#if !defined(CONFIG_ZMK_SPLIT) || defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <dt-bindings/zmk/pointing.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_ec_ms_data {
    struct sensor_value remainder[ZMK_KEYMAP_SENSORS_LEN][ZMK_KEYMAP_LAYERS_LEN];
    int32_t triggers[ZMK_KEYMAP_SENSORS_LEN][ZMK_KEYMAP_LAYERS_LEN];
};

static int behavior_ec_ms_accept_data(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event,
                                      const struct zmk_sensor_config *sensor_config,
                                      size_t channel_data_size,
                                      const struct zmk_sensor_channel_data *channel_data) {
    if (channel_data_size == 0 || channel_data == NULL) {
        return -EINVAL;
    }

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }

    struct behavior_ec_ms_data *data = dev->data;
    const struct sensor_value value = channel_data[0].value;

    int sensor_index = ZMK_SENSOR_POSITION_FROM_VIRTUAL_KEY_POSITION(event.position);
    struct sensor_value *rem = &data->remainder[sensor_index][event.layer];
    int32_t *trig = &data->triggers[sensor_index][event.layer];

    if ((value.val1 > 0 && rem->val1 < 0) || (value.val1 < 0 && rem->val1 > 0)) {
        rem->val1 = 0;
        rem->val2 = 0;
    }

    rem->val1 += value.val1;
    rem->val2 += value.val2;

    if (abs(rem->val2) >= 1000000) {
        rem->val1 += rem->val2 / 1000000;
        rem->val2 %= 1000000;
    }

    int trigger_degrees = 360 / sensor_config->triggers_per_rotation;
    int triggers = rem->val1 / trigger_degrees;

    rem->val1 %= trigger_degrees;
    *trig = triggers;

    LOG_DBG("ec_ms accept [sensor %d]: val1=%d, val2=%d, remainder=%d, triggers=%d",
            sensor_index, value.val1, value.val2, rem->val1, triggers);

    return 0;
}

static int behavior_ec_ms_process(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event,
                                  enum behavior_sensor_binding_process_mode mode) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }
    struct behavior_ec_ms_data *data = dev->data;
    
    int sensor_index = ZMK_SENSOR_POSITION_FROM_VIRTUAL_KEY_POSITION(event.position);
    int32_t triggers = data->triggers[sensor_index][event.layer];
    
    /* DISCARD時も正しくゼロクリアされる */
    data->triggers[sensor_index][event.layer] = 0; 

    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    if (triggers == 0) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

#if !defined(CONFIG_ZMK_SPLIT) || defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    uint32_t param = (triggers > 0) ? binding->param1 : binding->param2;

    int16_t h_wheel = MOVE_X_DECODE(param);
    int16_t wheel = MOVE_Y_DECODE(param);

    if (h_wheel > 0) h_wheel = 1;
    else if (h_wheel < 0) h_wheel = -1;

    if (wheel > 0) wheel = 1;
    else if (wheel < 0) wheel = -1;

    int abs_trig = (triggers > 0) ? triggers : -triggers;
    h_wheel *= abs_trig;
    wheel *= abs_trig;

    LOG_DBG("Encoder Mouse Scroll Sent: h_wheel=%d, wheel=%d", h_wheel, wheel);

    zmk_hid_mouse_scroll_set(h_wheel, wheel);
    zmk_endpoint_send_mouse_report();

    zmk_hid_mouse_scroll_set(0, 0);
    zmk_endpoint_send_mouse_report();

    return ZMK_BEHAVIOR_OPAQUE;
#else
    return ZMK_BEHAVIOR_TRANSPARENT;
#endif
}

static const struct behavior_driver_api behavior_encoder_mouse_scroll_driver_api = {
    .sensor_binding_accept_data = behavior_ec_ms_accept_data,
    .sensor_binding_process = behavior_ec_ms_process
};

#define EMS_INST(n)                                                                                 \
    static struct behavior_ec_ms_data behavior_ec_ms_data_##n = {};                                 \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_ec_ms_data_##n, NULL, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                            &behavior_encoder_mouse_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EMS_INST)