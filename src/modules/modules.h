// modules.h — pulls in every module header. Each module's .cpp registers itself
// via REGISTER_MODULE at static-init time; since all of src/ is compiled and
// linked directly (not as a pruned library), including the headers here is
// belt-and-suspenders to keep the registrars from being dropped and to give a
// single place to see everything that's wired in.
#pragma once

#include "wardrive/Wardrive.h"
#include "lora_sniffer/LoraSniffer.h"
#include "messenger/Messenger.h"
#include "ir/Ir.h"
#include "lorawan_fuzzer/LorawanFuzzer.h"
#include "wifi/Wifi.h"
#include "ble/Ble.h"
#include "subghz/SubGhz.h"
#include "rfid/Rfid.h"
#include "badusb/BadUsb.h"
