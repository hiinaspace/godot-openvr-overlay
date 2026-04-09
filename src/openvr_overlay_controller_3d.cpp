#include "openvr_overlay_controller_3d.h"
#include "openvr_overlay_origin_3d.h"
#include "openvr_math.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Map button name string → EVRButtonId bitmask
uint64_t OpenVROverlayController3D::_button_mask(const String &p_button) {
    if (p_button == "trigger")  return vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
    if (p_button == "grip")     return vr::ButtonMaskFromId(vr::k_EButton_Grip);
    if (p_button == "menu")     return vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu);
    if (p_button == "touchpad") return vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
    if (p_button == "a")        return vr::ButtonMaskFromId(vr::k_EButton_A);
    if (p_button == "system")   return vr::ButtonMaskFromId(vr::k_EButton_System);
    return 0;
}

void OpenVROverlayController3D::_bind_methods() {
    BIND_ENUM_CONSTANT(HAND_LEFT);
    BIND_ENUM_CONSTANT(HAND_RIGHT);

    ClassDB::bind_method(D_METHOD("set_hand", "hand"), &OpenVROverlayController3D::set_hand);
    ClassDB::bind_method(D_METHOD("get_hand"), &OpenVROverlayController3D::get_hand);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "hand", PROPERTY_HINT_ENUM, "Left,Right"),
        "set_hand", "get_hand");

    ClassDB::bind_method(D_METHOD("get_is_active"), &OpenVROverlayController3D::get_is_active);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_active", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY),
        "", "get_is_active");

    ClassDB::bind_method(D_METHOD("get_has_tracking_data"), &OpenVROverlayController3D::get_has_tracking_data);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_tracking_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY),
        "", "get_has_tracking_data");

    ClassDB::bind_method(D_METHOD("is_button_pressed", "button"), &OpenVROverlayController3D::is_button_pressed);
    ClassDB::bind_method(D_METHOD("get_trigger"), &OpenVROverlayController3D::get_trigger);
    ClassDB::bind_method(D_METHOD("get_axis", "index"), &OpenVROverlayController3D::get_axis);

    ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(Variant::STRING, "button_name")));
    ADD_SIGNAL(MethodInfo("button_released", PropertyInfo(Variant::STRING, "button_name")));
}

void OpenVROverlayController3D::_process(double /*delta*/) {
    if (!vr::VRSystem()) {
        m_is_active = false;
        m_has_tracking_data = false;
        return;
    }

    vr::ETrackedControllerRole role = (m_hand == HAND_LEFT)
        ? vr::TrackedControllerRole_LeftHand
        : vr::TrackedControllerRole_RightHand;

    // Try direct role lookup first; fall back to enumeration (needed in overlay mode)
    vr::TrackedDeviceIndex_t device_idx =
        vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(role);

    if (device_idx == vr::k_unTrackedDeviceIndexInvalid) {
        for (uint32_t i = 1; i < vr::k_unMaxTrackedDeviceCount; ++i) {
            if (vr::VRSystem()->GetTrackedDeviceClass(i) != vr::TrackedDeviceClass_Controller)
                continue;
            if (vr::VRSystem()->GetControllerRoleForTrackedDeviceIndex(i) == role) {
                device_idx = i;
                break;
            }
        }
    }

    if (device_idx == vr::k_unTrackedDeviceIndexInvalid) {
        m_is_active = false;
        m_has_tracking_data = false;
        return;
    }
    m_is_active = true;

    // Get pose
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding, 0.0f,
        poses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t &pose = poses[device_idx];
    m_has_tracking_data = pose.bPoseIsValid;

    if (m_has_tracking_data) {
        float world_scale = 1.0f;
        if (auto *origin = Object::cast_to<OpenVROverlayOrigin3D>(get_parent()))
            world_scale = origin->get_world_scale();

        Transform3D t = ovr_math::to_godot(pose.mDeviceToAbsoluteTracking);
        t.origin *= world_scale;
        set_transform(t);
    }

    // Controller state + button signals
    vr::VRControllerState_t new_state{};
    vr::VRSystem()->GetControllerState(device_idx, &new_state, sizeof(new_state));

    uint64_t pressed   = new_state.ulButtonPressed & ~m_prev_buttons;
    uint64_t released  = m_prev_buttons & ~new_state.ulButtonPressed;

    static const struct { const char *name; vr::EVRButtonId id; } k_buttons[] = {
        { "trigger",  vr::k_EButton_SteamVR_Trigger  },
        { "grip",     vr::k_EButton_Grip              },
        { "menu",     vr::k_EButton_ApplicationMenu   },
        { "touchpad", vr::k_EButton_SteamVR_Touchpad  },
        { "a",        vr::k_EButton_A                 },
        { "system",   vr::k_EButton_System            },
    };

    for (auto &b : k_buttons) {
        uint64_t mask = vr::ButtonMaskFromId(b.id);
        if (pressed  & mask) emit_signal("button_pressed",  String(b.name));
        if (released & mask) emit_signal("button_released", String(b.name));
    }

    m_state = new_state;
    m_prev_buttons = new_state.ulButtonPressed;
}

void OpenVROverlayController3D::set_hand(Hand p_hand) { m_hand = p_hand; }
OpenVROverlayController3D::Hand OpenVROverlayController3D::get_hand() const { return m_hand; }
bool OpenVROverlayController3D::get_is_active() const { return m_is_active; }
bool OpenVROverlayController3D::get_has_tracking_data() const { return m_has_tracking_data; }

bool OpenVROverlayController3D::is_button_pressed(const String &p_button) const {
    uint64_t mask = _button_mask(p_button);
    return mask && (m_state.ulButtonPressed & mask);
}

float OpenVROverlayController3D::get_trigger() const {
    return m_state.rAxis[1].x;
}

Vector2 OpenVROverlayController3D::get_axis(int p_index) const {
    if (p_index < 0 || p_index >= 5) return Vector2();
    return Vector2(m_state.rAxis[p_index].x, m_state.rAxis[p_index].y);
}
