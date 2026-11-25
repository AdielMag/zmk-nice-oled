#include "util.h"
#include <ctype.h>
#include <zephyr/kernel.h>

void to_uppercase(char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    str[i] = toupper(str[i]);
  }
}

void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]) {
  static lv_color_t cbuf_tmp[CANVAS_HEIGHT * CANVAS_HEIGHT];
  memcpy(cbuf_tmp, cbuf, sizeof(cbuf_tmp));

  lv_img_dsc_t img;
  img.data = (void *)cbuf_tmp;
  img.header.cf = LV_IMG_CF_TRUE_COLOR;
  img.header.w = CANVAS_HEIGHT;
  img.header.h = CANVAS_HEIGHT;

  /* The OLED is mounted rotated: the panel's long axis (128 px, screen x) is the keyboard's
   * vertical axis, the short axis (32 px, screen y) is its horizontal axis.
   *  - LEFT margin (keyboard view)  = blank rows at the start of the short axis: content moves +y
   *  - TOP margin (keyboard view)   = blank columns at the long-axis end: content moves -x
   * Both are done on the already rotated canvas buffer (lv_canvas_transform offsets were unreliable). */
  const int shift_y = CONFIG_NICE_OLED_DISPLAY_HEIGHT * CONFIG_NICE_OLED_LEFT_MARGIN_PERCENT / 100;
  const int shift_x = CONFIG_NICE_OLED_DISPLAY_WIDTH * CONFIG_NICE_OLED_TOP_MARGIN_PERCENT / 100;

  lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
  lv_canvas_transform(canvas, &img, 900, LV_IMG_ZOOM_NONE, -1, 0, CANVAS_HEIGHT / 2,
                      CANVAS_HEIGHT / 2, false);

  if (shift_x > 0 && shift_x < CANVAS_HEIGHT) {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
      lv_color_t *row = &cbuf[y * CANVAS_HEIGHT];
      memmove(row, row + shift_x, (CANVAS_HEIGHT - shift_x) * sizeof(lv_color_t));
      for (int x = CANVAS_HEIGHT - shift_x; x < CANVAS_HEIGHT; x++) {
        row[x] = LVGL_BACKGROUND;
      }
    }
  }
  if (shift_y > 0 && shift_y < CANVAS_HEIGHT) {
    memmove(&cbuf[shift_y * CANVAS_HEIGHT], &cbuf[0],
            (CANVAS_HEIGHT - shift_y) * CANVAS_HEIGHT * sizeof(lv_color_t));
    for (int i = 0; i < shift_y * CANVAS_HEIGHT; i++) {
      cbuf[i] = LVGL_BACKGROUND;
    }
  }
  if (shift_x > 0 || shift_y > 0) {
    lv_obj_invalidate(canvas);
  }
}

void draw_background(lv_obj_t *canvas) {
  lv_draw_rect_dsc_t rect_black_dsc;
  init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

  lv_canvas_draw_rect(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                      &rect_black_dsc);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color,
                    const lv_font_t *font, lv_text_align_t align) {
  lv_draw_label_dsc_init(label_dsc);
  label_dsc->color = color;
  label_dsc->font = font;
  label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
  lv_draw_rect_dsc_init(rect_dsc);
  rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color,
                   uint8_t width) {
  lv_draw_line_dsc_init(line_dsc);
  line_dsc->color = color;
  line_dsc->width = width;
}
