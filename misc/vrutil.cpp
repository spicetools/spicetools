#include "vrutil.h"
#include "util/logging.h"

namespace vrutil {
    static bool INITIALIZED = false;

    VRStatus STATUS = VRStatus::Disabled;
    vr::IVRSystem *SYSTEM = nullptr;
    vr::TrackedDeviceIndex_t INDEX_LEFT = ~0u;
    vr::TrackedDeviceIndex_t INDEX_RIGHT = ~0u;

    static std::string device_string(vr::TrackedDeviceIndex_t dev,
                                     vr::TrackedDeviceProperty prop) {
        uint32_t len = vr::VRSystem()->GetStringTrackedDeviceProperty(
                dev, prop, nullptr, 0, nullptr);
        if (len == 0) return "";
        char *buf = new char[len];
        vr::VRSystem()->GetStringTrackedDeviceProperty(
                dev, prop, buf, len, nullptr);
        std::string res = buf;
        delete[] buf;
        return res;
    }

    void init() {
        if (INITIALIZED) return;

        // initialize OpenVR
        vr::EVRInitError error = vr::VRInitError_None;
        SYSTEM = vr::VR_Init(&error, vr::VRApplication_Other);
        if (error != vr::VRInitError_None || !SYSTEM) {
            SYSTEM = nullptr;
            STATUS = VRStatus::Error;
            log_warning("vrutil", "unable to initialize: {}",
                        vr::VR_GetVRInitErrorAsEnglishDescription(error));
            return;
        }

        // get information
        auto driver = device_string(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
        auto serial = device_string(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
        log_info("vrutil", "initialized: {} - {}", driver, serial);

        // success
        INITIALIZED = true;
        STATUS = VRStatus::Running;
        scan(true);
    }

    void shutdown() {
        if (STATUS == VRStatus::Running) {
            vr::VR_Shutdown();
        }
        SYSTEM = nullptr;
        STATUS = VRStatus::Disabled;
    }

    void scan(bool log) {
        if (STATUS != VRStatus::Running) return;
        for (vr::TrackedDeviceIndex_t index = 0; index < vr::k_unMaxTrackedDeviceCount; ++index) {
            auto dclass = SYSTEM->GetTrackedDeviceClass(index);
            switch (dclass) {
                case vr::TrackedDeviceClass_Controller: {
                    auto role = SYSTEM->GetControllerRoleForTrackedDeviceIndex(index);
                    switch (role) {
                        case vr::TrackedControllerRole_Invalid:
                            if (log) log_warning("vrutil", "invalid controller on index {}", index);
                            break;
                        case vr::TrackedControllerRole_LeftHand:
                            if (log) log_info("vrutil", "detected left controller on index {}", index);
                            INDEX_LEFT = index;
                            break;
                        case vr::TrackedControllerRole_RightHand:
                            if (log) log_info("vrutil", "detected right controller on index {}", index);
                            INDEX_RIGHT = index;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case vr::TrackedDeviceClass_GenericTracker: {
                    if (log) log_info("vrutil", "detected tracker on index {}", index);
                    break;
                }
                default:
                    break;
            }

        }
    }

    bool get_hmd_pose(vr::TrackedDevicePose_t *pose) {
        if (STATUS != VRStatus::Running) {
            *pose = vr::TrackedDevicePose_t {};
            pose->bPoseIsValid = false;
            return false;
        }
        SYSTEM->GetDeviceToAbsoluteTrackingPose(
                vr::TrackingUniverseStanding,
                0,
                pose, 1);
        return true;
    }

    bool get_con_pose(vr::TrackedDeviceIndex_t index,
                      vr::TrackedDevicePose_t *pose,
                      vr::VRControllerState_t *state) {
        if (STATUS != VRStatus::Running || index == ~0u) {
            *state = vr::VRControllerState_t {};
            *pose = vr::TrackedDevicePose_t {};
            pose->bPoseIsValid = false;
            return false;
        }
        SYSTEM->GetControllerStateWithPose(
                vr::TrackingUniverseStanding,
                index, state, sizeof(vr::VRControllerState_t), pose);
        return true;
    }
}
