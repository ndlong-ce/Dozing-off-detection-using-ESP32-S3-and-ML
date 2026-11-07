#pragma once
// Logic canh bao:
//  - mat nham (P(closed) > CLOSED_THRESH) lien tiep CLOSED_FRAMES khung -> alarm (LED sang)
//  - alarm keo dai lien tuc >= BUZZER_DELAY_MS -> buzz (buzzer keu); mat mo thi tat ngay
#include <stdint.h>

struct AlarmState {
    int closed_cnt = 0;
    bool alarm = false;
    bool buzz = false;
    int alarm_ms = 0;      // da canh bao lien tuc bao nhieu ms (de debug thoi diem buzzer)
};

class AlarmLogic {
public:
    AlarmState update(bool face, float p_closed, int64_t now_us);

private:
    int closed_cnt_ = 0;
    int64_t alarm_since_us_ = 0;   // thoi diem bat dau canh bao lien tuc (0 = chua canh bao)
};
