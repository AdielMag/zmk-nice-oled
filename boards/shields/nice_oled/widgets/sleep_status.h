/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_sleep_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *art;
    lv_obj_t *chg; // per-half charging indicator on the idle art (NULL until init)
    bool chg_left, chg_right;
};

int zmk_widget_sleep_status_init(struct zmk_widget_sleep_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_sleep_status_obj(struct zmk_widget_sleep_status *widget);
// Show a bolt for each half that is on USB power (charging) while the idle art is up.
void zmk_widget_sleep_status_set_charging(struct zmk_widget_sleep_status *widget, bool left,
                                          bool right);