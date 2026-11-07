#include "eye_monitor.h"

#include <math.h>
#include <vector>
#include "esp_log.h"
#include "config.h"
#include "shared.h"

static const char *TAG = "eye";

void EyeMonitor::init()
{
    eye_.init();
    detector_ = new HumanFaceDetect();
}

EyeFrameResult EyeMonitor::analyze(camera_fb_t *fb)
{
    EyeFrameResult r;

    dl::image::img_t img;
    img.data = fb->buf;
    img.width = fb->width;
    img.height = fb->height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE;

    auto &faces = detector_->run(img);
    r.face = !faces.empty();
    if (!r.face) return r;

    // keypoint: [l_eye x,y, l_mouth x,y, nose x,y, r_eye x,y, r_mouth x,y]
    const auto &f = faces.front();
    const std::vector<int> &k = f.keypoint;
    r.lx = k[0]; r.ly = k[1]; r.rx = k[6]; r.ry = k[7];
    r.side = EYE_SIDE_RATIO * hypotf(r.rx - r.lx, r.ry - r.ly);
    if (r.side < 8.0f) {
        r.face = false;
        return r;
    }

    EyeOut left, right;
    eye_.predict(fb->buf, fb->width, fb->height, r.lx, r.ly, r.side, left);
    eye_.predict(fb->buf, fb->width, fb->height, r.rx, r.ry, r.side, right);
    publish_eye_debug(0, left.crop, left.l0, left.l1);
    publish_eye_debug(1, right.crop, right.l0, right.l1);
    r.p_closed = 0.5f * (left.p_closed + right.p_closed);
    ESP_LOGI(TAG, "eye pl=%.2f pr=%.2f  logits L=[%.2f %.2f] R=[%.2f %.2f]",
             left.p_closed, right.p_closed, left.l0, left.l1, right.l0, right.l1);

    if (f.box.size() >= 4) {
        r.bx0 = f.box[0]; r.by0 = f.box[1]; r.bx1 = f.box[2]; r.by1 = f.box[3];
        r.have_box = true;
    }
    return r;
}
