#include <cmath>
#include "external/openvr/headers/openvr.h"
#include "external/linalg.h"

namespace vrutil {

    enum class VRStatus {
        Disabled, Error, Running,
    };

    extern VRStatus STATUS;
    extern vr::IVRSystem *SYSTEM;
    extern vr::TrackedDeviceIndex_t INDEX_LEFT;
    extern vr::TrackedDeviceIndex_t INDEX_RIGHT;

    void init();
    void shutdown();
    void scan(bool log = false);

    bool get_hmd_pose(vr::TrackedDevicePose_t *pose);
    bool get_con_pose(vr::TrackedDeviceIndex_t index,
                      vr::TrackedDevicePose_t *pose,
                      vr::VRControllerState_t *state);

    /*
     * Util
     */

    inline linalg::aliases::float3 get_translation(vr::HmdMatrix34_t &m) {
        return linalg::aliases::float3 {
                m.m[0][3],
                m.m[1][3],
                m.m[2][3],
        };
    }

    inline linalg::aliases::float3 get_direction_back(vr::HmdMatrix34_t &m) {
        return linalg::normalize(linalg::aliases::float3 {
            m.m[0][2],
            m.m[1][2],
            m.m[2][2],
        });
    }

    inline linalg::aliases::float3 get_direction_front(vr::HmdMatrix34_t &m) {
        return -get_direction_back(m);
    }

    inline linalg::aliases::float3 get_direction_up(vr::HmdMatrix34_t &m) {
        return linalg::normalize(linalg::aliases::float3 {
                m.m[0][1],
                m.m[1][1],
                m.m[2][1],
        });
    }

    inline linalg::aliases::float3 get_direction_down(vr::HmdMatrix34_t &m) {
        return -get_direction_up(m);
    }

    inline linalg::aliases::float3 get_direction_right(vr::HmdMatrix34_t &m) {
        return linalg::normalize(linalg::aliases::float3 {
                m.m[0][0],
                m.m[1][0],
                m.m[2][0],
        });
    }

    inline linalg::aliases::float3 get_direction_left(vr::HmdMatrix34_t &m) {
        return -get_direction_right(m);
    }

    template<class T>
    inline linalg::aliases::float4 get_rotation(T &m) {
        linalg::aliases::float4 q;
        q.w = sqrtf(fmaxf(0, 1 + m[0][0] + m[1][1] + m[2][2])) * 0.5f;
        q.x = sqrtf(fmaxf(0, 1 + m[0][0] - m[1][1] - m[2][2])) * 0.5f;
        q.y = sqrtf(fmaxf(0, 1 - m[0][0] + m[1][1] - m[2][2])) * 0.5f;
        q.z = sqrtf(fmaxf(0, 1 - m[0][0] - m[1][1] + m[2][2])) * 0.5f;
        q.x = copysignf(q.x, m[2][1] - m[1][2]);
        q.y = copysignf(q.y, m[0][2] - m[2][0]);
        q.z = copysignf(q.z, m[1][0] - m[0][1]);
        return q;
    }

    inline linalg::aliases::float4 get_rotation(
            float pitch, float yaw, float roll) {
        return linalg::aliases::float4 {
            sinf(roll/2) * cosf(pitch/2) * cosf(yaw/2) - cosf(roll/2) * sinf(pitch/2) * sinf(yaw/2),
            cosf(roll/2) * sinf(pitch/2) * cosf(yaw/2) + sinf(roll/2) * cosf(pitch/2) * sinf(yaw/2),
            cosf(roll/2) * cosf(pitch/2) * sinf(yaw/2) - sinf(roll/2) * sinf(pitch/2) * cosf(yaw/2),
            cosf(roll/2) * cosf(pitch/2) * cosf(yaw/2) + sinf(roll/2) * sinf(pitch/2) * sinf(yaw/2),
        };
    }

    inline linalg::aliases::float3 orthogonal(
            linalg::aliases::float3 v) {
        auto x = std::fabs(v.x);
        auto y = std::fabs(v.y);
        auto z = std::fabs(v.z);
        linalg::aliases::float3 o;
        if (x < y) {
            if (x < z) o = linalg::aliases::float3(1, 0, 0);
            else o = linalg::aliases::float3(0, 0, 1);
        } else {
            if (y < z) o = linalg::aliases::float3(0, 1, 0);
            else linalg::aliases::float3(0, 0, 1);
        }
        return linalg::cross(v, o);
    }

    inline linalg::aliases::float4 get_rotation(
            linalg::aliases::float3 v1,
            linalg::aliases::float3 v2) {
        auto dot = linalg::dot(v1, v2);
        auto scl = sqrtf(linalg::length2(v1) * linalg::length2(v2));
        if (dot / scl == -1) {
            auto v = linalg::normalize(orthogonal(v1));
            return linalg::aliases::float4 {
                v.x, v.y, v.z, 0,
            };
        }
        auto cross = linalg::cross(v1, v2);
        return linalg::normalize(linalg::aliases::float4 {
            cross.x, cross.y, cross.z, dot + scl,
        });
    }

    inline linalg::aliases::float3 intersect_point(
            linalg::aliases::float3 ray_dir,
            linalg::aliases::float3 ray_point,
            linalg::aliases::float3 plane_normal,
            linalg::aliases::float3 plane_point) {
        auto delta = ray_point - plane_point;
        auto delta_dot = linalg::dot(delta, plane_normal);
        auto ray_dot = linalg::dot(ray_dir, plane_normal);
        return ray_point - ray_dir * (delta_dot / ray_dot);
    }
}
