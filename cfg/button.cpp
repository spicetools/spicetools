#include "button.h"

#include "rawinput/rawinput.h"
#include "util/logging.h"
#include "util/utils.h"

const char *ButtonAnalogTypeStr[] = {
        "None",
        "Positive",
        "Negative",
        "Hat Up",
        "Hat Upright",
        "Hat Right",
        "Hat Downright",
        "Hat Down",
        "Hat Downleft",
        "Hat Left",
        "Hat Upleft",
        "Hat Neutral",
        "MIDI Control Precision",
        "MIDI Control Single",
        "MIDI Control On/Off",
        "MIDI Pitch Down",
        "MIDI Pitch Up",
};

std::string Button::getVKeyString() {
    switch (this->getVKey() % 256) {
        case 0x01:
            return "Left MB";
        case 0x02:
            return "Right MB";
        case 0x04:
            return "Middle MB";
        case 0x05:
            return "X1 MB";
        case 0x06:
            return "X2 MB";
        case 0x08:
            return "Backspace";
        case 0x09:
            return "Tab";
        case 0x0C:
            return "Clear";
        case 0x0D:
            return "Enter";
        case 0x10:
            return "Shift";
        case 0x11:
            return "Ctrl";
        case 0x12:
            if (this->getVKey() > 255)
                return "AltGr";
            else
                return "Alt";
        case 0x13:
            return "Pause";
        case 0x14:
            return "Caps Lock";
        case 0x1B:
            return "Escape";
        case 0x20:
            return "Space";
        case 0x21:
            return "Page Up";
        case 0x22:
            return "Page Down";
        case 0x23:
            return "End";
        case 0x24:
            return "Home";
        case 0x25:
            return "Left";
        case 0x26:
            return "Up";
        case 0x27:
            return "Right";
        case 0x28:
            return "Down";
        case 0x2C:
            return "Prt Scr";
        case 0x2D:
            return "Insert";
        case 0x2E:
            return "Delete";
        case 0x30:
            return "0";
        case 0x31:
            return "1";
        case 0x32:
            return "2";
        case 0x33:
            return "3";
        case 0x34:
            return "4";
        case 0x35:
            return "5";
        case 0x36:
            return "6";
        case 0x37:
            return "7";
        case 0x38:
            return "8";
        case 0x39:
            return "9";
        case 0x41:
            return "A";
        case 0x42:
            return "B";
        case 0x43:
            return "C";
        case 0x44:
            return "D";
        case 0x45:
            return "E";
        case 0x46:
            return "F";
        case 0x47:
            return "G";
        case 0x48:
            return "H";
        case 0x49:
            return "I";
        case 0x4A:
            return "J";
        case 0x4B:
            return "K";
        case 0x4C:
            return "L";
        case 0x4D:
            return "M";
        case 0x4E:
            return "N";
        case 0x4F:
            return "O";
        case 0x50:
            return "P";
        case 0x51:
            return "Q";
        case 0x52:
            return "R";
        case 0x53:
            return "S";
        case 0x54:
            return "T";
        case 0x55:
            return "U";
        case 0x56:
            return "V";
        case 0x57:
            return "W";
        case 0x58:
            return "X";
        case 0x59:
            return "Y";
        case 0x5A:
            return "Z";
        case 0x5B:
            return "Left Windows";
        case 0x5C:
            return "Right Windows";
        case 0x5D:
            return "Apps";
        case 0x60:
            return "Num 0";
        case 0x61:
            return "Num 1";
        case 0x62:
            return "Num 2";
        case 0x63:
            return "Num 3";
        case 0x64:
            return "Num 4";
        case 0x65:
            return "Num 5";
        case 0x66:
            return "Num 6";
        case 0x67:
            return "Num 7";
        case 0x68:
            return "Num 8";
        case 0x69:
            return "Num 9";
        case 0x6A:
            return "*";
        case 0x6B:
            return "+";
        case 0x6C:
            return "Seperator";
        case 0x6D:
            return "-";
        case 0x6E:
            return ".";
        case 0x6F:
            return "/";
        case 0x70:
            return "F1";
        case 0x71:
            return "F2";
        case 0x72:
            return "F3";
        case 0x73:
            return "F4";
        case 0x74:
            return "F5";
        case 0x75:
            return "F6";
        case 0x76:
            return "F7";
        case 0x77:
            return "F8";
        case 0x78:
            return "F9";
        case 0x79:
            return "F10";
        case 0x7A:
            return "F11";
        case 0x7B:
            return "F12";
        case 0x7C:
            return "F13";
        case 0x7D:
            return "F14";
        case 0x7E:
            return "F15";
        case 0x7F:
            return "F16";
        case 0x80:
            return "F17";
        case 0x81:
            return "F18";
        case 0x82:
            return "F19";
        case 0x83:
            return "F20";
        case 0x84:
            return "F21";
        case 0x85:
            return "F22";
        case 0x86:
            return "F23";
        case 0x87:
            return "F24";
        case 0x90:
            return "Num Lock";
        case 0x91:
            return "Scroll Lock";
        case 0xA0:
            return "Left Shift";
        case 0xA1:
            return "Right Shift";
        case 0xA2:
            return "Left Control";
        case 0xA3:
            return "Right Control";
        case 0xA4:
            return "Left Menu";
        case 0xA5:
            return "Right Menu";
        default:

            // check win API
            char keyName[128];
            if (GetKeyNameText((LONG) (MapVirtualKey(vKey, MAPVK_VK_TO_VSC) << 16), keyName, 128))
                return std::string(keyName);
            return "Unknown";
    }
}

std::string Button::getDisplayString(rawinput::RawInputManager* manager) {

    // get VKey string
    auto vKey = (uint16_t) this->getVKey();
    std::string vKeyString = fmt::format("{:#x}", vKey);

    // device must be existing
    if (this->device_identifier.empty() && vKey == 0xFF) {
        return "";
    }

    if (this->isNaive()) {
        return this->getVKeyString() + " (Naive, " + vKeyString + ")";
    } else {
        auto device = manager->devices_get(this->device_identifier);
        if (!device) {
            return "Device missing (" + vKeyString + ")";
        }

        std::lock_guard<std::mutex> lock(*device->mutex);

        switch (device->type) {
            case rawinput::MOUSE: {
                const char *btn = "Unknown";
                static const char *MOUSE_NAMES[] = {
                        "Left Mouse",
                        "Right Mouse",
                        "Middle Mouse",
                        "Mouse 1",
                        "Mouse 2",
                        "Mouse 3",
                        "Mouse 4",
                        "Mouse 5",
                };
                if (vKey < sizeof(MOUSE_NAMES)) {
                    btn = MOUSE_NAMES[vKey];
                }
                return fmt::format("{} ({})", btn, device->desc);
            }
            case rawinput::KEYBOARD:
                return this->getVKeyString() + " (" + device->desc + ")";
            case rawinput::HID: {
                auto hid = device->hidInfo;
                switch (this->analog_type) {
                    case BAT_NONE:
                        if (vKey < hid->button_caps_names.size())
                            return hid->button_caps_names[vKey] + " (" + device->desc + ")";
                        else
                            return "Invalid button (" + device->desc + ")";
                    case BAT_NEGATIVE:
                    case BAT_POSITIVE: {
                        const char *sign = this->analog_type == BAT_NEGATIVE ? "-" : "+";
                        if (vKey < hid->value_caps_names.size()) {
                            return hid->value_caps_names[vKey] + sign + " (" + device->desc + ")";
                        } else {
                            return "Invalid analog (" + device->desc + ")";
                        }
                    }
                    case BAT_HS_UP:
                        return "Hat Up (" + device->desc + ")";
                    case BAT_HS_UPRIGHT:
                        return "Hat UpRight (" + device->desc + ")";
                    case BAT_HS_RIGHT:
                        return "Hat Right (" + device->desc + ")";
                    case BAT_HS_DOWNRIGHT:
                        return "Hat DownRight (" + device->desc + ")";
                    case BAT_HS_DOWN:
                        return "Hat Down (" + device->desc + ")";
                    case BAT_HS_DOWNLEFT:
                        return "Hat DownLeft (" + device->desc + ")";
                    case BAT_HS_LEFT:
                        return "Hat Left (" + device->desc + ")";
                    case BAT_HS_UPLEFT:
                        return "Hat UpLeft (" + device->desc + ")";
                    case BAT_HS_NEUTRAL:
                        return "Hat Neutral (" + device->desc + ")";
                    default:
                        return "Unknown analog type (" + device->desc + ")";
                }
            }
            case rawinput::MIDI:
                switch (this->analog_type) {
                    case BAT_NONE:
                        return "MIDI " + vKeyString + " (" + device->desc + ")";
                    case BAT_MIDI_CTRL_PRECISION:
                        return "MIDI PREC " + vKeyString + " (" + device->desc + ")";
                    case BAT_MIDI_CTRL_SINGLE:
                        return "MIDI CTRL " + vKeyString + " (" + device->desc + ")";
                    case BAT_MIDI_CTRL_ONOFF:
                        return "MIDI ONOFF " + vKeyString + " (" + device->desc + ")";
                    case BAT_MIDI_PITCH_DOWN:
                        return "MIDI Pitch Down (" + device->desc + ")";
                    case BAT_MIDI_PITCH_UP:
                        return "MIDI Pitch Up (" + device->desc + ")";
                    default:
                        return "MIDI Unknown " + vKeyString + " (" + device->desc + ")";
                }
            case rawinput::PIUIO_DEVICE:
                return "PIUIO " + vKeyString;
            case rawinput::DESTROYED:
                return "Device unplugged (" + vKeyString + ")";
            default:
                return "Unknown device type (" + vKeyString + ")";
        }
    }
}

#define HAT_SWITCH_INCREMENT (1.f / 7)

void Button::getHatSwitchValues(float analog_state, ButtonAnalogType* buffer) {
    // rawinput converts neutral hat switch values to a negative value
    if (analog_state < 0.f) {
        buffer[0] = BAT_HS_NEUTRAL;
        buffer[1] = BAT_NONE;
        buffer[2] = BAT_NONE;
        return;
    }
    if (analog_state < 0 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_UP;
        buffer[1] = BAT_NONE;
        buffer[2] = BAT_NONE;
        return;
    }
    if (analog_state < 1 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_UPRIGHT;
        buffer[1] = BAT_HS_UP;
        buffer[2] = BAT_HS_RIGHT;
        return;
    }
    if (analog_state < 2 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_RIGHT;
        buffer[1] = BAT_NONE;
        buffer[2] = BAT_NONE;
        return;
    }
    if (analog_state < 3 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_DOWNRIGHT;
        buffer[1] = BAT_HS_RIGHT;
        buffer[2] = BAT_HS_DOWN;
        return;
    }
    if (analog_state < 4 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_DOWN;
        buffer[1] = BAT_NONE;
        buffer[2] = BAT_NONE;
        return;
    }
    if (analog_state < 5 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_DOWNLEFT;
        buffer[1] = BAT_HS_DOWN;
        buffer[2] = BAT_HS_LEFT;
        return;
    }
    if (analog_state < 6 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_LEFT;
        buffer[1] = BAT_NONE;
        buffer[2] = BAT_NONE;
        return;
    }
    if (analog_state < 7 * HAT_SWITCH_INCREMENT + 0.001f) {
        buffer[0] = BAT_HS_UPLEFT;
        buffer[1] = BAT_HS_LEFT;
        buffer[2] = BAT_HS_UP;
        return;
    }
    
    buffer[0] = BAT_HS_NEUTRAL;
    buffer[1] = BAT_NONE;
    buffer[2] = BAT_NONE;
    return;
}
