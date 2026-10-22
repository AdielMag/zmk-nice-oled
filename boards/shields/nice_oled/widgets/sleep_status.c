/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#include "sleep_status.h"
#include "util.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

LV_IMG_DECLARE(vim_32x128);
LV_IMG_DECLARE(vim_68x160);
LV_IMG_DECLARE(sleep_oled);
LV_IMG_DECLARE(dog_sit1_90);
LV_IMG_DECLARE(dog_sit2_90);

#if !IS_ENABLED(CONFIG_NICE_EPAPER_ON)
// idle screen: Luna sitting (2-frame animation) on a blank full screen
static const lv_img_dsc_t *idle_luna_imgs[] = {&dog_sit1_90, &dog_sit2_90};
#endif

static void set_sleep_img(struct zmk_widget_sleep_status *widget,
                          struct zmk_activity_state_changed ev) {
    switch (ev.state) {
    case ZMK_ACTIVITY_ACTIVE:
        LOG_DBG("ACTIVITY EVENT ACTIVE");
        lv_obj_add_flag(widget->art, LV_OBJ_FLAG_HIDDEN);
        break;
    case ZMK_ACTIVITY_IDLE:
        LOG_DBG("ACTIVITY EVENT IDLE");
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE)
        lv_obj_clear_flag(widget->art, LV_OBJ_FLAG_HIDDEN);
#else
        lv_obj_add_flag(widget->art, LV_OBJ_FLAG_HIDDEN);
#endif
        break;
    case ZMK_ACTIVITY_SLEEP:
        LOG_DBG("ACTIVITY EVENT SLEEP");
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
        lv_obj_clear_flag(widget->art, LV_OBJ_FLAG_HIDDEN);
#else
        lv_obj_add_flag(widget->art, LV_OBJ_FLAG_HIDDEN);
#endif
        break;
    default:
        LOG_WRN("Unhandled activity state: %d", ev.state);
    }
}

static void sleep_status_update_cb(struct zmk_activity_state_changed ev) {
    struct zmk_widget_sleep_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_sleep_img(widget, ev); }
}

static struct zmk_activity_state_changed sleep_status_get_state(const zmk_event_t *eh) {
    // eh is NULL on the initial widget update: fall back to the current state
    const struct zmk_activity_state_changed *ev = eh ? as_zmk_activity_state_changed(eh) : NULL;
    if (ev) {
        return *ev;
    }
    return (struct zmk_activity_state_changed){.state = zmk_activity_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_sleep_status, struct zmk_activity_state_changed,
                            sleep_status_update_cb, sleep_status_get_state)

ZMK_SUBSCRIPTION(widget_sleep_status, zmk_activity_state_changed);

int zmk_widget_sleep_status_init(struct zmk_widget_sleep_status *widget, lv_obj_t *parent) {
    sys_slist_append(&widgets, &widget->node);
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    widget->art = lv_img_create(widget->obj);
    lv_img_set_src(widget->art, &vim_68x160);
    lv_obj_align(widget->art, LV_ALIGN_TOP_LEFT, 0, 0);
#else
    // opaque 128x32 panel that covers the normal status screen
    widget->art = lv_obj_create(widget->obj);
    lv_obj_remove_style_all(widget->art);
    lv_obj_set_size(widget->art, 128, 32);
    lv_obj_set_style_bg_color(widget->art, LVGL_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(widget->art, LV_OPA_COVER, 0);
    lv_obj_clear_flag(widget->art, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(widget->art, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *luna = lv_animimg_create(widget->art);
    lv_animimg_set_src(luna, (const void **)idle_luna_imgs, ARRAY_SIZE(idle_luna_imgs));
    lv_animimg_set_duration(luna, 960);
    lv_animimg_set_repeat_count(luna, LV_ANIM_REPEAT_INFINITE);
    lv_obj_center(luna);
    lv_animimg_start(luna);
#endif

    lv_obj_add_flag(widget->art, LV_OBJ_FLAG_HIDDEN);

    widget_sleep_status_init();
    return 0;
}

lv_obj_t *zmk_widget_sleep_status_obj(struct zmk_widget_sleep_status *widget) {
    return widget->obj;
}
