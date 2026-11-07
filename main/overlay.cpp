#include "overlay.h"

#include "config.h"

// ---- ve khung len anh RGB565 (byte cao truoc, giong gray_at) ----
#define COL_RED    0xF800
#define COL_GREEN  0x07E0
#define COL_YELLOW 0xFFE0

static inline void put_px(uint8_t *buf, int w, int h, int x, int y, uint16_t c)
{
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    uint8_t *p = buf + 2 * (y * w + x);
    p[0] = (uint8_t)(c >> 8);
    p[1] = (uint8_t)(c & 0xFF);
}

static void draw_rect(uint8_t *buf, int w, int h, int x0, int y0, int x1, int y1, uint16_t c)
{
    for (int t = 0; t < 2; t++) {                       // day 2 pixel
        for (int x = x0; x <= x1; x++) { put_px(buf, w, h, x, y0 + t, c); put_px(buf, w, h, x, y1 - t, c); }
        for (int y = y0; y <= y1; y++) { put_px(buf, w, h, x0 + t, y, c); put_px(buf, w, h, x1 - t, y, c); }
    }
}

void overlay_draw(uint8_t *buf, int w, int h, const EyeFrameResult &r, bool alarm)
{
    if (r.face) {
        uint16_t col = (r.p_closed > CLOSED_THRESH) ? COL_RED : COL_GREEN;
        if (r.have_box) draw_rect(buf, w, h, r.bx0, r.by0, r.bx1, r.by1, COL_YELLOW);
        int hs = (int)(r.side / 2);
        draw_rect(buf, w, h, (int)r.lx - hs, (int)r.ly - hs, (int)r.lx + hs, (int)r.ly + hs, col);
        draw_rect(buf, w, h, (int)r.rx - hs, (int)r.ry - hs, (int)r.rx + hs, (int)r.ry + hs, col);
    }
    if (alarm) {   // vien do quanh toan khung hinh khi bao dong
        draw_rect(buf, w, h, 0, 0, w - 1, h - 1, COL_RED);
    }
}
