#include "core/powerSave.h"
#include "core/config.h"
#include <interface.h>

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   Initial setup for JC-ESP32P4-M3 (Headless WebUI AP Hotspot)
***************************************************************************************/
void _setup_gpio() {
    bruceConfig.startupApp = "WebUI";
    bruceConfig.wifiAp.ssid = "BruceNet-P4";
    bruceConfig.wifiAp.pwd = "brucenet";
}

int getBattery() { return 100; }
bool isCharging() { return true; }
void _setBrightness(uint8_t brightval) {}
void InputHandler(void) {}
void powerOff() {}
void checkReboot() {}
