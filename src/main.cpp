// Huginn-RF — entry point.
//
// Boot sequence: bring up the device + services, mount SD, load YAML config,
// init the LoRa radio and GPS, then hand control to the menu which walks the
// module registry. Modules self-register at static-init, so adding a feature
// never touches this file.
#include <M5Cardputer.h>

#include "core/Display.h"
#include "core/Input.h"
#include "core/SdCard.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/Menu.h"
#include "core/Module.h"
#include "hal/Lora.h"
#include "hal/Gps.h"
#include "modules/modules.h"   // keeps all module registrars linked

static Display s_display;
static Input   s_input;
static SdCard  s_sd;
static Config  s_config;
static Logger  s_logger;
static Lora    s_lora;
static Gps     s_gps;
static Menu    s_menu;
static Context s_ctx;

static void splash(const char* line1, const char* line2) {
    s_display.clear();
    s_display.header("Huginn-RF " HUGINN_VERSION);
    s_display.line(0, line1, 0xFFFF);
    if (line2) s_display.line(1, line2, 0x8410);
    s_display.present();
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // --- Config (SD) ---
    bool sdOk = s_sd.begin();
    s_config.load(s_sd);  // falls back to defaults if SD/file missing

    // --- Display ---
    s_display.begin((uint8_t)s_config.getInt("core.brightness", 160));
    splash(sdOk ? "SD mounted" : "NO SD (defaults)",
           "booting services...");

    // --- Input ---
    s_input.begin();

    // --- GPS ---
    if (s_config.getBool("gps.enabled", true))
        s_gps.begin(s_config.getInt("gps.baud", 9600));

    // --- Logger (needs SD + GPS) ---
    s_logger.begin(&s_sd, &s_gps, s_config.getBool("core.log_to_sd", true));

    // --- LoRa radio ---
    bool loraOk = s_lora.begin(
        s_config.getFloat("radio.frequency_mhz", 915.0f),
        s_config.getFloat("radio.bandwidth_khz", 125.0f),
        (uint8_t)s_config.getInt("radio.spreading_factor", 7),
        (int8_t)s_config.getInt("radio.tx_power_dbm", 14));

    // --- Context wiring ---
    s_ctx.display = &s_display;
    s_ctx.input   = &s_input;
    s_ctx.sd      = &s_sd;
    s_ctx.config  = &s_config;
    s_ctx.log     = &s_logger;
    s_ctx.lora    = &s_lora;
    s_ctx.gps     = &s_gps;
    // Honor the config ack; otherwise modules prompt on first use.
    s_ctx.authorized = s_config.getBool("core.authorized_ack", false);

    s_logger.info("Boot: SD=%d LoRa=%d region=%s", sdOk, loraOk,
                  s_config.getString("radio.region", "US915").c_str());
    if (!loraOk) s_logger.warn("SX1262 not found — check cap wiring in board.h");

    splash(loraOk ? "LoRa OK" : "LoRa NOT found",
           "press any key...");
    delay(400);
}

void loop() {
    // Menu::run never returns; it owns the UI event loop.
    s_menu.run(s_ctx);
}
