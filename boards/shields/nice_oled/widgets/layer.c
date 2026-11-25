#include "layer.h"
#include <fonts.h>
#include <zephyr/kernel.h>
#include <string.h>

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_CENTER);
#else
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#endif // CONFIG_NICE_EPAPER_ON

    char text[10] = {};

    if (state->layer_label == NULL) {
        sprintf(text, "Layer %i", state->layer_index);
    } else {
        strncpy(text, state->layer_label, 9);
        to_uppercase(text);
    }

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
#endif
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y, 68, &label_dsc, text);
#else
    // 128x32 OLED behind a case cutout: visible width is 22px. Mono fonts: 12 -> 6px/char,
    // 8 -> 4px/char. Names are cut to 5 chars (5 * 4 = 20px), centred, bottom-aligned to a 16px slot.
    text[5] = '\0';
    size_t len = strlen(text);
    int y = CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y;
    if (len <= 3) {
        label_dsc.font = &pixel_operator_mono_12;
        y += 3;
    } else {
        label_dsc.font = &pixel_operator_mono_8;
        y += 6;
    }
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_X, y, 22, &label_dsc, text);
#endif
}
