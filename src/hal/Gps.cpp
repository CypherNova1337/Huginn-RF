#include "Gps.h"
#include "board.h"
#include <TinyGPSPlus.h>

static TinyGPSPlus s_gps;
static HardwareSerial s_serial(GPS_UART_NUM);

void Gps::begin(uint32_t baud) {
    s_serial.begin(baud, SERIAL_8N1, GPS_PIN_RX, GPS_PIN_TX);
}

void Gps::update() {
    while (s_serial.available()) {
        if (s_gps.encode(s_serial.read())) {
            if (s_gps.location.isValid()) {
                fix_.lat = s_gps.location.lat();
                fix_.lon = s_gps.location.lng();
                fix_.valid = true;
            }
            if (s_gps.altitude.isValid())  fix_.altMeters = s_gps.altitude.meters();
            if (s_gps.speed.isValid())     fix_.speedKmph = s_gps.speed.kmph();
            if (s_gps.satellites.isValid())fix_.satellites = s_gps.satellites.value();
            if (s_gps.date.isValid()) {
                fix_.year = s_gps.date.year();
                fix_.month = s_gps.date.month();
                fix_.day = s_gps.date.day();
            }
            if (s_gps.time.isValid()) {
                fix_.hour = s_gps.time.hour();
                fix_.minute = s_gps.time.minute();
                fix_.second = s_gps.time.second();
            }
        }
    }
}

String Gps::isoTime() const {
    char buf[24];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
             fix_.year, fix_.month, fix_.day, fix_.hour, fix_.minute, fix_.second);
    return String(buf);
}
