#pragma once
// Buzzer: keu lien tuc (BUZZER_FREQ_HZ) khi duoc yeu cau, chay trong task rieng
void buzzer_init();             // cau hinh chan, tat buzzer, tao task
void buzzer_request(bool on);   // true = yeu cau keu, false = tat ngay
bool buzzer_requested();        // trang thai yeu cau hien tai
void buzzer_test(int ms);       // keu thu trong ms mili giay (khong phu thuoc nhan dien mat), dung de kiem tra phan cung
