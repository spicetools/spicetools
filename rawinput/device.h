#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <vector>

#include <windows.h>

extern "C" {
#include <hidusage.h>
#include <hidsdi.h>
}

#include "util/unique_plain_ptr.h"

#include "sextet.h"

namespace rawinput {

    enum DeviceType {
        DESTROYED,
        UNKNOWN,
        MOUSE,
        KEYBOARD,
        HID,
        MIDI,
        SEXTET_OUTPUT,
        PIUIO_DEVICE,
    };

    enum MouseKeys {
        MOUSEBTN_LEFT = 0,
        MOUSEBTN_RIGHT = 1,
        MOUSEBTN_MIDDLE = 2,
        MOUSEBTN_1 = 3,
        MOUSEBTN_2 = 4,
        MOUSEBTN_3 = 5,
        MOUSEBTN_4 = 6,
        MOUSEBTN_5 = 7,
    };

    enum MousePos {
        MOUSEPOS_X = 0,
        MOUSEPOS_Y = 1,
        MOUSEPOS_WHEEL = 2,
    };

    struct DeviceInfo {
        std::string devclass;
        std::string subclass;
        std::string protocol;
        std::string guid_str;
        GUID guid;
    };

    struct DeviceMouseInfo {
        bool key_states[16];
        bool key_states_bind[16];
        double key_up[16];
        double key_down[16];
        long pos_x, pos_y;
        long pos_wheel;
    };

    struct DeviceKeyboardInfo {
        bool key_states[1024];
        double key_up[1024];
        double key_down[1024];
    };

    enum class HIDDriver {
        Default,
        PacDrive,
    };

    struct HIDTouchPoint {
        DWORD id;
        bool down;
        float x, y;
        size_t ttl;
        uint64_t last_report; // unix time in ms
    };

    struct HIDTouchScreenData {
        bool valid = false;
        bool parsed_elements = false;
        size_t remaining_contact_count = 0;
        std::vector<int> elements_contact_count;
        std::vector<int> elements_contact_identifier;
        std::vector<int> elements_x;
        std::vector<int> elements_y;
        std::vector<int> elements_width;
        std::vector<int> elements_height;
        std::vector<int> elements_pressed;
        std::vector<int> elements_pressure;
        std::vector<HIDTouchPoint> touch_points;
    };

    struct DeviceHIDInfo {
        HANDLE handle;
        _HIDP_CAPS caps;
        HIDD_ATTRIBUTES attributes;
        HIDDriver driver = HIDDriver::Default;
        HIDTouchScreenData touch {};

        std::string usage_name;
        util::unique_plain_ptr<_HIDP_PREPARSED_DATA> preparsed_data;
        UINT preparsed_size;

        std::vector<HIDP_BUTTON_CAPS> button_caps_list;
        std::vector<std::string> button_caps_names;
        std::vector<HIDP_BUTTON_CAPS> button_output_caps_list;
        std::vector<std::string> button_output_caps_names;
        std::vector<HIDP_VALUE_CAPS> value_caps_list;
        std::vector<std::string> value_caps_names;
        std::vector<HIDP_VALUE_CAPS> value_output_caps_list;
        std::vector<std::string> value_output_caps_names;
        std::vector<std::vector<bool>> button_states;
        std::vector<std::vector<double>> button_up, button_down;
        std::vector<std::vector<bool>> button_output_states;
        std::vector<float> value_states;
        std::vector<LONG> value_states_raw;
        std::vector<float> value_output_states;

        // for config binding function
        std::vector<float> bind_value_states;
    };

    struct DeviceMIDIInfo {
        std::vector<bool> states;
        std::vector<uint8_t> states_events;
        std::vector<bool> bind_states;
        std::vector<uint8_t> velocity; // 16*128 x 7 bit resolution
        bool freeze;
        std::vector<uint16_t> controls_precision; // 16*32 14 bit resolution
        std::vector<uint16_t> controls_precision_bind;
        std::vector<bool> controls_precision_msb;
        std::vector<bool> controls_precision_lsb;
        std::vector<bool> controls_precision_set;
        std::vector<uint8_t> controls_single; // 16*44 7 bit resolution
        std::vector<uint8_t> controls_single_bind;
        std::vector<bool> controls_single_set;
        std::vector<bool> controls_onoff; // 16*6 1 bit resolution :)
        std::vector<bool> controls_onoff_bind;
        std::vector<bool> controls_onoff_set;
        uint16_t pitch_bend; // 14 bit resolution
    };

    class PIUIO;

    struct Device {
        size_t id;
        std::string name;
        std::string desc;
        HANDLE handle = INVALID_HANDLE_VALUE;
        DeviceType type = UNKNOWN;
        DeviceInfo info;
        std::mutex *mutex;
        std::mutex *mutex_out;
        bool updated = true;
        bool output_pending = true;
        bool output_enabled = false;
        DeviceMouseInfo *mouseInfo = nullptr;
        DeviceKeyboardInfo *keyboardInfo = nullptr;
        DeviceHIDInfo *hidInfo = nullptr;
        DeviceMIDIInfo *midiInfo = nullptr;
        SextetDevice *sextetInfo = nullptr;
        PIUIO* piuioDev = nullptr;
        double input_time = 0.0;
        double input_hz = 0.f;
        double input_hz_max = 0.f;
    };
}
