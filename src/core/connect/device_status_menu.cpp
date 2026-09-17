#include "device_status.h"
#include "device_status_service.h"
#include "core/display.h"
#include "core/mykeyboard.h"

namespace {
using namespace device_status;
void offerMenu() {
    if (!openStatusOffer()) { displayError("WLAN oder freier Platz fehlt", true); return; }
    while (true) {
        const auto v = statusView();
        if (!v.offering) break;
        drawMainBorderWithTitle("Status koppeln");
        setTftDisplay(10, 55, bruceConfig.priColor, 1, bruceConfig.bgColor);
        tft.println("Meine ID:");
        tft.println(idLabel(v.local));
        tft.println("Schluessel (60 Sekunden):");
        const String key = keyLabel(v.offer);
        tft.println(key.substring(0, 16));
        tft.println(key.substring(16));
        tft.println("Am anderen Geraet eingeben.");
        printFootnote("ESC: Abbrechen");
        if (v.pending) {
            bool approved = false;
            std::vector<Option> confirm = {
                {"Statuskopplung bestaetigen", [&]() { approved = approveStatusPeer(); }},
                {"Ablehnen", []() { closeStatusOffer(); }},
            };
            const String title = "Anfrage " + idLabel(v.requester);
            loopOptions(confirm, MENU_TYPE_SUBMENU, title.c_str());
            if (approved) displaySuccess("Status gekoppelt", true);
            break;
        }
        if (check(EscPress)) break;
        delay(150);
    }
    closeStatusOffer();
}
void foundMenu() {
    bool back = false;
    while (!back) {
        const auto v = statusView();
        std::vector<Option> items;
        for (const auto &f : v.found) if (f.used) {
            const Id id = f.id;
            items.push_back({idLabel(id), [id]() {
                Key key{};
                const String text = hex_keyboard("", 32, "32-stelliger Schluessel", true);
                if (!parseKey(text.c_str(), key)) { displayError("Ungueltiger Schluessel", true); return; }
                bool confirmed = false;
                std::vector<Option> confirm = {
                    {"Nur Status koppeln", [&]() { confirmed = true; }},
                    {"Abbrechen", []() {}},
                };
                loopOptions(confirm, MENU_TYPE_SUBMENU, idLabel(id).c_str());
                if (!confirmed) return;
                if (requestStatusPeer(id, key)) displayInfo("Am Ziel bestaetigen (60s)", true);
                else displayError("Nicht erreichbar oder belegt", true);
            }});
        }
        if (items.empty()) items.push_back({"Keine Geraete gefunden", []() {}});
        items.push_back({"Aktualisieren", []() {}});
        items.push_back({"Zurueck", [&]() { back = true; }});
        if (loopOptions(items, MENU_TYPE_SUBMENU, "Geraete im WLAN") < 0) break;
    }
}
void peersMenu() {
    bool back = false;
    while (!back) {
        const auto v = statusView();
        std::vector<Option> items;
        for (const auto &p : v.peers) if (p.used) {
            String label = idLabel(p.id) + ": " + (p.connected ? statusLabel(p.status) : "Warte...");
            items.push_back({label, []() {}});
        }
        if (items.empty()) items.push_back({"Keine Kopplung", []() {}});
        items.push_back({"Aktualisieren", []() {}});
        items.push_back({"Zurueck", [&]() { back = true; }});
        if (loopOptions(items, MENU_TYPE_SUBMENU, "Peer-Status") < 0) break;
    }
}
void sendMenu() {
    std::vector<Option> items;
    for (auto status : {Status::Ready, Status::Busy, Status::LowBattery, Status::Hello})
        items.push_back({statusLabel(status), [status]() { publishDeviceStatus(status); }});
    loopOptions(items, MENU_TYPE_SUBMENU, "Nur Status senden");
}
} // namespace

void deviceStatusMenu() {
    bool back = false;
    while (!back) {
        const auto v = device_status::statusView();
        std::vector<Option> items = {
            {v.online ? "Im WLAN auffindbar" : "Offline: WLAN verbinden", []() {}},
            {"Geraete in der Naehe", foundMenu},
            {"Kopplung anbieten", offerMenu},
            {"Gekoppelte Geraete", peersMenu},
            {"Status senden", sendMenu},
            {"Alle trennen", device_status::disconnectStatusPeers},
            {"Zurueck", [&]() { back = true; }},
        };
        if (!v.running) { displayError("Statusdienst nicht gestartet", true); return; }
        if (loopOptions(items, MENU_TYPE_SUBMENU, "Geraetestatus") < 0) break;
    }
}
