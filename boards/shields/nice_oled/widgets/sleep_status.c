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

/*
 * Charging indicator at the top of the (portrait-mounted) idle screen: "L<bolt>" on the left,
 * "R<bolt>" on the right, each only while that half is on USB power. The art panel is in
 * native display coords (128x32 landscape); the user's "up" is native +x, the user's "left"
 * is native y = 0. Glyphs below are written as the user sees them and rotated when drawn.
 */
/* Visible window (native coords) once the case cutout is accounted for, same numbers as the main
 * screen margins: native y starts at the left margin, native x ends at the top margin. */
#define VIS_Y0 (CONFIG_NICE_OLED_DISPLAY_HEIGHT * CONFIG_NICE_OLED_LEFT_MARGIN_PERCENT / 100)
#define VIS_X1                                                                                         (CONFIG_NICE_OLED_DISPLAY_WIDTH -                                                                   CONFIG_NICE_OLED_DISPLAY_WIDTH * CONFIG_NICE_OLED_TOP_MARGIN_PERCENT / 100)
#define VIS_W (CONFIG_NICE_OLED_DISPLAY_HEIGHT - VIS_Y0) // visible width in the user's view
#define CHG_ROW_H 10                                      // one "L<bolt>" row, bolt is 8 tall
#define CHG_PH (2 * CHG_ROW_H - 1) // portrait height of the strip (= native width): L row + R row
#define CHG_PW VIS_W               // portrait width (= native height)
static lv_color_t chg_buf[CHG_PH * CHG_PW];

static const char *const chg_bolt[] = {
    "....##.", "...##..", "..##...", ".######", "######.", "...##..", "..##...", ".##....",
};
static const char *const chg_L[] = {"#....", "#....", "#....", "#....", "#....", "#....", "#####"};
static const char *const chg_R[] = {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"};

static void chg_glyph(lv_obj_t *canvas, const char *const *rows, int nrows, int px0, int py0) {
    for (int y = 0; y < nrows; y++) {
        for (int x = 0; rows[y][x]; x++) {
            if (rows[y][x] == '#') {
                // portrait (px, py) -> native (CHG_PH - 1 - py, px)
                lv_canvas_set_px_color(canvas, CHG_PH - 1 - (py0 + y), px0 + x, LVGL_FOREGROUND);
            }
        }
    }
}

static void chg_draw_side(lv_obj_t *canvas, const char *const *letter, int py0) {
    chg_glyph(canvas, letter, 7, 1, py0 + 1);
    chg_glyph(canvas, chg_bolt, ARRAY_SIZE(chg_bolt), 8, py0);
}

static void chg_redraw(struct zmk_widget_sleep_status *widget) {
    if (widget->chg == NULL) {
        return;
    }
    if (!widget->chg_left && !widget->chg_right) {
        lv_obj_add_flag(widget->chg, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_canvas_fill_bg(widget->chg, LVGL_BACKGROUND, LV_OPA_COVER);
    if (widget->chg_left) {
        chg_draw_side(widget->chg, chg_L, 0); /* top row (user view) */
    }
    if (widget->chg_right) {
        chg_draw_side(widget->chg, chg_R, CHG_ROW_H); /* row below */
    }
    lv_obj_clear_flag(widget->chg, LV_OBJ_FLAG_HIDDEN);
}
#endif

void zmk_widget_sleep_status_set_charging(struct zmk_widget_sleep_status *widget, bool left,
                                          bool right) {
    if (widget->chg_left == left && widget->chg_right == right) {
        return;
    }
    widget->chg_left = left;
    widget->chg_right = right;
#if !IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    chg_redraw(widget);
#endif
}

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
    /* sprite is 24 x 32 native; its art covers native y 8..28 (21 rows). Centre that art in the
     * visible window (VIS_Y0..31 by 0..VIS_X1-1). */
    lv_obj_align(luna, LV_ALIGN_TOP_LEFT, (VIS_X1 - 24) / 2, VIS_Y0 + (VIS_W - 21) / 2 - 8);
    lv_animimg_start(luna);

    // charging strip at the user's top (native right edge of the visible window), clear of Luna
    widget->chg = lv_canvas_create(widget->art);
    lv_canvas_set_buffer(widget->chg, chg_buf, CHG_PH, CHG_PW, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(widget->chg, LV_ALIGN_TOP_LEFT, VIS_X1 - CHG_PH, VIS_Y0);
    chg_redraw(widget); // state may have arrived before init
#endif

    lv_obj_add_flag(widget->art, LV_OBJ_FLAG_HIDDEN);

    widget_sleep_status_init();
    return 0;
}

lv_obj_t *zmk_widget_sleep_status_obj(struct zmk_widget_sleep_status *widget) {
    return widget->obj;
}
