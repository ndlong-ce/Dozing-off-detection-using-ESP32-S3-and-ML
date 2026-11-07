#include "alarm_logic.h"

#include "config.h"

AlarmState AlarmLogic::update(bool face, float p_closed, int64_t now_us)
{
    if (face && p_closed > CLOSED_THRESH) closed_cnt_++; else closed_cnt_ = 0;
    bool alarm = closed_cnt_ >= CLOSED_FRAMES;

    if (alarm) { if (alarm_since_us_ == 0) alarm_since_us_ = now_us; } else { alarm_since_us_ = 0; }
    bool buzz = alarm && (now_us - alarm_since_us_) >= (int64_t)BUZZER_DELAY_MS * 1000;

    AlarmState st;
    st.closed_cnt = closed_cnt_;
    st.alarm = alarm;
    st.buzz = buzz;
    st.alarm_ms = alarm ? (int)((now_us - alarm_since_us_) / 1000) : 0;
    return st;
}
