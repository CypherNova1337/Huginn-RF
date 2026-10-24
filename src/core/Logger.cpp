#include "Logger.h"
#include "SdCard.h"
#include "../hal/Gps.h"
#include <FS.h>
#include <stdarg.h>

void Logger::begin(SdCard* sd, Gps* gps, bool toSd) {
    sd_ = sd; gps_ = gps; toSd_ = toSd;
    Serial.begin(115200);
}

static void vfmt(char* buf, size_t n, const char* fmt, va_list ap) {
    vsnprintf(buf, n, fmt, ap);
}

void Logger::write(const char* level, const char* tag, const char* msg) {
    char stamp[48];
    uint32_t s = millis() / 1000;
    snprintf(stamp, sizeof(stamp), "[%6lu.%03lu]",
             (unsigned long)s, (unsigned long)(millis() % 1000));

    Serial.printf("%s %-4s %-10s %s\n", stamp, level, tag, msg);

    if (toSd_ && sd_ && sd_->mounted()) {
        File f = sd_->fs().open("/huginn/huginn.log", FILE_APPEND);
        if (f) {
            f.printf("%s %-4s %-10s %s", stamp, level, tag, msg);
            if (gps_ && gps_->hasFix()) {
                auto fix = gps_->fix();
                f.printf(" @%.6f,%.6f", fix.lat, fix.lon);
            }
            f.print('\n');
            f.close();
        }
    }
}

void Logger::info(const char* fmt, ...) {
    char b[160]; va_list ap; va_start(ap, fmt); vfmt(b, sizeof(b), fmt, ap); va_end(ap);
    write("INFO", "-", b);
}
void Logger::warn(const char* fmt, ...) {
    char b[160]; va_list ap; va_start(ap, fmt); vfmt(b, sizeof(b), fmt, ap); va_end(ap);
    write("WARN", "-", b);
}
void Logger::tx(const char* module, const char* fmt, ...) {
    char b[160]; va_list ap; va_start(ap, fmt); vfmt(b, sizeof(b), fmt, ap); va_end(ap);
    write("TX", module, b);
}
