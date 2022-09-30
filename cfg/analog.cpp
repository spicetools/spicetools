#include "analog.h"

#include <numeric>

#include <math.h>

#include "rawinput/rawinput.h"
#include "util/logging.h"
#include "util/time.h"
#include "util/utils.h"

std::string Analog::getDisplayString(rawinput::RawInputManager *manager) {

    // device must be existing
    if (this->device_identifier.empty()) {
        return "";
    }

    // get index string
    auto index = this->getIndex();
    std::string indexString = fmt::format("{:#x}", index);

    // get device
    auto device = manager->devices_get(this->device_identifier);
    if (!device) {
        return "Device missing (" + indexString + ")";
    }

    // return string based on device type
    switch (device->type) {
        case rawinput::MOUSE: {
            const char *name;
            switch (index) {
                case rawinput::MOUSEPOS_X:
                    name = "X";
                    break;
                case rawinput::MOUSEPOS_Y:
                    name = "Y";
                    break;
                case rawinput::MOUSEPOS_WHEEL:
                    name = "Scroll Wheel";
                    break;
                default:
                    name = "?";
                    break;
            }
            return fmt::format("{} ({})", name, device->desc);
        }
        case rawinput::HID: {
            auto hid = device->hidInfo;
            if (index < hid->value_caps_names.size()) {
                return hid->value_caps_names[index] + " (" + device->desc + ")";
            }
            return "Invalid Axis (" + indexString + ")";
        }
        case rawinput::MIDI: {
            auto midi = device->midiInfo;
            if (index < midi->controls_precision.size()) {
                return "MIDI PREC " + indexString + " (" + device->desc + ")";
            } else if (index < midi->controls_precision.size() + midi->controls_single.size()) {
                return "MIDI CTRL " + indexString + " (" + device->desc + ")";
            } else if (index < midi->controls_precision.size() + midi->controls_single.size()
                                                               + midi->controls_onoff.size())
            {
                return "MIDI ONOFF " + indexString + " (" + device->desc + ")";
            } else if (index == midi->controls_precision.size() + midi->controls_single.size()
                                                                + midi->controls_onoff.size())
            {
                return "MIDI Pitch Bend (" + device->desc + ")";
            } else {
                return "MIDI Unknown " + indexString + " (" + device->desc + ")";
            }
        }
        case rawinput::DESTROYED:
            return "Device unplugged (" + indexString + ")";
        default:
            return "Unknown Axis (" + indexString + ")";
    }
}

float Analog::getSmoothedValue(float raw_rads) {
    auto now = get_performance_milliseconds();

    // prevent extremely frequent polling
    if ((now - vector_history.at(vector_history_index).time_in_ms) < 0.9) {
        return smoothed_last_state;
    }

    // calculate derived values for the newly-read analog value
    vector_history_index = (vector_history_index + 1) % vector_history.size();
    auto &current  = vector_history.at(vector_history_index);
    current.time_in_ms = now;
    current.sine = sin(raw_rads);
    current.cosine = cos(raw_rads);

    // calculated the weighted sum of sines and cosines
    auto sines = 0.f;
    auto cosines = 0.f;
    for (auto &vector : vector_history) {
        auto time_diff = now - vector.time_in_ms;
        // time from QPC should never roll backwards, but just in case
        if (time_diff < 0.f) {
            time_diff = 0.f;
        }

        // the weight falls of linearly; value from 24ms ago counts as half, 48ms ago counts as 0
        double weight = (-time_diff / 48.f) + 1.f;
        if (weight > 0.f) {
            sines += weight * vector.sine;
            cosines += weight * vector.cosine;
        }
    }

    // add a tiny bit so that cosine is never 0.0f when fed to atan2
    if (cosines == 0.f) {
        cosines = std::nextafter(0.f, 1.f);
    }

    // average for angles:
    // arctan[(sum of sines of all angles) / (sum of cosines of all angles)]
    // atan2 will give [-pi, +pi], so normalize to make [0, 2pi]
    smoothed_last_state = normalizeAngle(atan2(sines, cosines));
    return smoothed_last_state;
}

float Analog::calculateAngularDifference(float old_rads, float new_rads) {
    float delta = new_rads - old_rads;

    // assumes value doesn't change more than PI (180 deg) compared to last poll
    if (std::abs(delta) < M_PI) {
        return delta;
    } else {
        // use the coterminal angle instead
        if (delta < 0.f) {
            return M_TAU + delta;
        } else {
            return -(M_TAU - delta);
        }
    }
}

float Analog::applyAngularSensitivity(float raw_rads) {
    float delta = calculateAngularDifference(previous_raw_rads, raw_rads);
    previous_raw_rads = raw_rads;
    adjusted_rads = normalizeAngle(adjusted_rads + (delta * sensitivity));
    return adjusted_rads;
}

float Analog::normalizeAngle(float rads) {
    // normalizes radian value into [0, 2pi] range.
    // for small angles, this is MUCH faster than fmodf.
    float angle = rads;
    while (angle > M_TAU) {
        angle -= M_TAU;
    }
    while (angle < 0.f) {
        angle += M_TAU;
    }
    return angle;
}
