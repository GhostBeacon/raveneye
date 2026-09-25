#include "keys.h"

#include <M5Cardputer.h>

#include "core.h"

static constexpr uint32_t REPEAT_DELAY_MS = 450;
static constexpr uint32_t REPEAT_EVERY_MS = 90;

static Keys held;          // zuletzt gedrueckter Zustand (fuer Wiederholung)
static bool heldRepeatable = false;
static uint32_t pressedAt = 0;
static uint32_t lastRepeat = 0;

static Keys fromState(const Keyboard_Class::KeysState& st) {
    Keys k;
    k.enter = st.enter;
    k.del = st.del;
    k.tab = st.tab;
    k.fn = st.fn;
    k.chars.assign(st.word.begin(), st.word.end());
    for (char c : k.chars) {
        if (c == ';') k.up = true;
        else if (c == '.') k.down = true;
        else if (c == ',') k.left = true;
        else if (c == '/') k.right = true;
        else if (c == '`') k.esc = true;
    }
    k.back = k.esc || k.del;
    return k;
}

bool keys::poll(Keys& k) {
    M5Cardputer.update();
    auto& kb = M5Cardputer.Keyboard;
    uint32_t now = millis();

    if (kb.isChange()) {
        if (!kb.isPressed()) {
            heldRepeatable = false;
            return false;
        }
        k = fromState(kb.keysState());
        held = k;
        // Nur Pfeile und Loeschen wiederholen - Enter oder Zeichen nicht
        heldRepeatable = (k.up || k.down || k.left || k.right || k.del) && !k.enter;
        pressedAt = lastRepeat = now;
        if (core::wake()) return false;  // hat nur das Display geweckt
        return true;
    }

    if (heldRepeatable && kb.isPressed() && now - pressedAt > REPEAT_DELAY_MS &&
        now - lastRepeat > REPEAT_EVERY_MS) {
        lastRepeat = now;
        k = held;
        k.repeat = true;
        core::wake();
        return true;
    }
    return false;
}
