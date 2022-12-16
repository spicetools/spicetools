#include "drs.h"

#include <windows.h>
#include <thread>

#include "avs/game.h"
#include "games/game.h"
#include "util/detour.h"
#include "util/logging.h"
#include "util/memutils.h"
#include "misc/vrutil.h"

#pragma pack(push)

typedef struct {
    union {
        struct {
            WORD unk1;
            WORD unk2;
            WORD device_id;
            WORD vid;
            WORD pid;
            WORD pvn;
            WORD max_point_num;
        };
        uint8_t raw[2356];
    };
} dev_info_t;

typedef struct {
    DWORD cid;
    DWORD type;
    DWORD unused;
    DWORD y;
    DWORD x;
    DWORD height;
    DWORD width;
    DWORD unk8;
} touch_data_t;

#pragma pack(pop)

enum touch_type {
    TS_DOWN = 1,
    TS_MOVE = 2,
    TS_UP = 3,
};

void *user_data = nullptr;
void (*touch_callback)(
        dev_info_t *dev_info,
        const touch_data_t *touch_data,
        int touch_points,
        int unk1,
        const void *user_data);

namespace games::drs {

	void* TouchSDK_Constructor(void* in) {
		return in;
	}

	bool TouchSDK_SendData(dev_info_t*,
	        unsigned char * const, int, unsigned char * const,
	        unsigned char* output, int output_size) {

        // fake success
        if (output_size >= 4) {
			output[0] = 0xfc;
			output[1] = 0xa5;
		}
		return true;
	}

	bool TouchSDK_SetSignalInit(dev_info_t*, int) {
		return true;
	}

	void TouchSDK_Destructor(void* This) {
	}

	int TouchSDK_GetYLedTotal(dev_info_t*, int) {
		return 53;
	}

	int TouchSDK_GetXLedTotal(dev_info_t*, int) {
		return 41;
	}

	bool TouchSDK_DisableTouch(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_DisableDrag(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_DisableWheel(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_DisableRightClick(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_SetMultiTouchMode(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_EnableTouchWidthData(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_EnableRawData(dev_info_t*, int) {
		return true;
	}

	bool TouchSDK_SetAllEnable(dev_info_t*, bool, int) {
		return true;
	}

	int TouchSDK_GetTouchDeviceCount(void* This) {
		return 1;
	}

	unsigned int TouchSDK_GetTouchSDKVersion(void) {
		return 0x01030307;
	}

	int TouchSDK_InitTouch(void* This, dev_info_t *devices, int max_devices, void* touch_event_cb,
	        void* hotplug_callback, void* userdata) {

        // fake touch device
        memset(devices, 0, sizeof(devices[0].raw));
        devices[0].unk1 = 0x1122;
        devices[0].unk2 = 0x3344;
        devices[0].device_id = 0;
        devices[0].vid = 0xDEAD;
        devices[0].pid = 0xBEEF;
        devices[0].pvn = 0xC0DE;
        devices[0].max_point_num = 16;

        // remember provided callback and userdata
        touch_callback = (decltype(touch_callback)) touch_event_cb;
        user_data = userdata;

        // success
		return 1;
	}

    char DRS_TAPELED[38 * 49][3] {};
    bool VR_STARTED = false;
    linalg::aliases::float3 VR_SCALE(100, 100, 1.f);
    linalg::aliases::float3 VR_OFFSET(0.f, 0.f, -0.1f);
    float VR_ROTATION = 0.f;
    VRFoot VR_FOOTS[2] {
            {1},
            {2},
    };

    inline DWORD scale_double_to_xy(double val) {
        return static_cast<DWORD>(val * 32768);
    }

    inline DWORD scale_double_to_height(double val) {
        return static_cast<DWORD>(val * 1312);
    }

    inline DWORD scale_double_to_width(double val) {
        return static_cast<DWORD>(val * 1696);
    }

    void fire_touches(drs_touch_t *events, size_t event_count) {

        // check callback first
        if (!touch_callback) {
            return;
        }

        // generate touch data
        auto game_touches = std::make_unique<touch_data_t[]>(event_count);
        for (size_t i = 0; i < event_count; i++) {

            // initialize touch value
            game_touches[i].cid = (DWORD) events[i].id;
            game_touches[i].unk8 = 0;

            // copy scaled values
            game_touches[i].x = scale_double_to_xy(events[i].x);
            game_touches[i].y = scale_double_to_xy(events[i].y);
            game_touches[i].width = scale_double_to_width(events[i].width);
            game_touches[i].height = scale_double_to_height(events[i].height);

            // decide touch type
            switch(events[i].type) {
                case DRS_DOWN:
                    game_touches[i].type = TS_DOWN;
                    break;
                case DRS_UP:
                    game_touches[i].type = TS_UP;
                    break;
                case DRS_MOVE:
                    game_touches[i].type = TS_MOVE;
                    break;
                default:
                    break;
            }
        }

        // build device information
        dev_info_t dev;
        dev.unk1 = 0;
        dev.unk2 = 0;
        dev.device_id = 0;
        dev.vid = 0xDEAD;
        dev.pid = 0xBEEF;
        dev.pvn = 0xC0DE;
        dev.max_point_num = 16;

        // fire callback
        touch_callback(&dev, game_touches.get(), (int) event_count, 0, user_data);
    }

    uint32_t VRFoot::get_index() {
        if (index == ~0u) {
            if (id == 1) return vrutil::INDEX_LEFT;
            if (id == 2) return vrutil::INDEX_RIGHT;
        }
        return index;
    }

    linalg::aliases::float3 VRFoot::to_world(linalg::aliases::float3 pos) {
        pos = linalg::aliases::float3(-pos.z, pos.x, pos.y);
        const float deg_to_rad = (float) (1 / 180.f * M_PI);
        auto s = sinf(VR_ROTATION * deg_to_rad);
        auto c = cosf(VR_ROTATION * deg_to_rad);
        pos = linalg::aliases::float3(pos.x * c - pos.y * s,
                                      pos.x * s + pos.y * c,
                                      pos.z);
        return pos * VR_SCALE + VR_OFFSET;
    }

    void start_vr() {
        if (vrutil::STATUS != vrutil::VRStatus::Running) return;
        if (!VR_STARTED) {
            VR_STARTED = true;
            std::thread t([] {
                log_info("drs", "starting VR thread");

                // dance floor plane
                const linalg::aliases::float3 plane_normal(0, 0, 1);
                const linalg::aliases::float3 plane_point(0, 0, 0);
                const float touch_width = 1.f;
                const float touch_height = 1.f;

                // main loop
                while (vrutil::STATUS == vrutil::VRStatus::Running) {

                    // iterate foots
                    for (auto &foot : VR_FOOTS) {
                        vr::TrackedDevicePose_t pose;
                        vr::VRControllerState_t state;
                        vrutil::get_con_pose(foot.get_index(), &pose, &state);

                        // only accept valid poses
                        if (pose.bPoseIsValid) {
                            auto length = std::max(foot.length, 0.001f);

                            // get components
                            auto translation = foot.to_world(vrutil::get_translation(
                                    pose.mDeviceToAbsoluteTracking));
                            auto direction = -linalg::qzdir(linalg::qmul(
                                    vrutil::get_rotation(pose.mDeviceToAbsoluteTracking.m),
                                    foot.rotation));
                            direction = linalg::aliases::float3 {
                                    -direction.z, direction.x, direction.y
                            };

                            // get intersection point
                            auto intersection = vrutil::intersect_point(
                                    direction, translation,
                                    plane_normal, plane_point);
                            auto distance = linalg::distance(
                                    translation, intersection);

                            // update event details
                            foot.height = std::max(0.f, translation.z - intersection.z);
                            foot.event.id = foot.id;
                            foot.event.x = (intersection.x / 38.f) * touch_width;
                            foot.event.y = (intersection.y / 49.f) * touch_height;
                            if (translation.z > intersection.z) {
                                foot.event.width = foot.size_base
                                        + foot.size_scale * std::max(0.f, 1.f - distance / length);
                            } else {
                                foot.event.width = foot.size_base + foot.size_scale;
                            }
                            foot.event.height = foot.event.width;

                            // check if controller points down
                            if (direction.z < 0) {

                                // check if plane in range
                                if (distance <= length) {

                                    // check previous event
                                    switch (foot.event.type) {
                                        case DRS_UP:

                                            // generate down event
                                            foot.event.type = DRS_DOWN;
                                            break;

                                        case DRS_DOWN:
                                        case DRS_MOVE:

                                            // generate move event
                                            foot.event.type = DRS_MOVE;
                                            break;

                                        default:
                                            break;
                                    }

                                    // send event
                                    drs::fire_touches(&foot.event, 1);
                                    continue;
                                }
                            }

                            // foot not intersecting with plane
                            switch (foot.event.type) {
                                case DRS_DOWN:
                                case DRS_MOVE:

                                    // generate up event
                                    foot.event.type = DRS_UP;
                                    drs::fire_touches(&foot.event, 1);
                                    break;

                                case DRS_UP:
                                default:
                                    break;
                            }
                        }
                    }

                    // slow down
                    vrutil::scan();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                VR_STARTED = false;
                return nullptr;
            });
            t.detach();
        }
    }

    DRSGame::DRSGame() : Game("DANCERUSH") {
    }

    void DRSGame::attach() {
        Game::attach();

        // TouchSDK hooks
        detour::iat("??0TouchSDK@@QEAA@XZ",
                (void *) &TouchSDK_Constructor, avs::game::DLL_INSTANCE);
        detour::iat("?SendData@TouchSDK@@QEAA_NU_DeviceInfo@@QEAEH1HH@Z",
                (void *) &TouchSDK_SendData, avs::game::DLL_INSTANCE);
        detour::iat("?SetSignalInit@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_SetSignalInit, avs::game::DLL_INSTANCE);
        detour::iat("??1TouchSDK@@QEAA@XZ",
                (void *) &TouchSDK_Destructor, avs::game::DLL_INSTANCE);
        detour::iat("?GetYLedTotal@TouchSDK@@QEAAHU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_GetYLedTotal, avs::game::DLL_INSTANCE);
        detour::iat("?GetXLedTotal@TouchSDK@@QEAAHU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_GetXLedTotal, avs::game::DLL_INSTANCE);
        detour::iat("?DisableTouch@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_DisableTouch, avs::game::DLL_INSTANCE);
        detour::iat("?DisableDrag@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_DisableDrag, avs::game::DLL_INSTANCE);
        detour::iat("?DisableWheel@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_DisableWheel, avs::game::DLL_INSTANCE);
        detour::iat("?DisableRightClick@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_DisableRightClick, avs::game::DLL_INSTANCE);
        detour::iat("?SetMultiTouchMode@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_SetMultiTouchMode, avs::game::DLL_INSTANCE);
        detour::iat("?EnableTouchWidthData@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_EnableTouchWidthData, avs::game::DLL_INSTANCE);
        detour::iat("?EnableRawData@TouchSDK@@QEAA_NU_DeviceInfo@@H@Z",
                (void *) &TouchSDK_EnableRawData, avs::game::DLL_INSTANCE);
        detour::iat("?SetAllEnable@TouchSDK@@QEAA_NU_DeviceInfo@@_NH@Z",
                (void *) &TouchSDK_SetAllEnable, avs::game::DLL_INSTANCE);
        detour::iat("?GetTouchDeviceCount@TouchSDK@@QEAAHXZ",
                (void *) &TouchSDK_GetTouchDeviceCount, avs::game::DLL_INSTANCE);
        detour::iat("?GetTouchSDKVersion@TouchSDK@@QEAAIXZ",
                (void *) &TouchSDK_GetTouchSDKVersion, avs::game::DLL_INSTANCE);
        detour::iat("?InitTouch@TouchSDK@@QEAAHPEAU_DeviceInfo@@HP6AXU2@PEBU_TouchPointData@@HHPEBX@ZP6AX1_N3@ZPEAX@Z",
                (void *) &TouchSDK_InitTouch, avs::game::DLL_INSTANCE);

        // VR
        if (vrutil::STATUS == vrutil::VRStatus::Running) {
            start_vr();
        }
    }

    void DRSGame::detach() {
        Game::detach();
    }
}
