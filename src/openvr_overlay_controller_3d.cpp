#include "openvr_overlay_controller_3d.h"
#include "openvr_overlay_origin_3d.h"
#include "openvr_math.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

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

    ADD_SIGNAL(MethodInfo("button_pressed",  PropertyInfo(Variant::STRING, "button_name")));
    ADD_SIGNAL(MethodInfo("button_released", PropertyInfo(Variant::STRING, "button_name")));
}

void OpenVROverlayController3D::_ready() {
    set_process(true);
}

void OpenVROverlayController3D::_notification(int p_what) {
    if (p_what != NOTIFICATION_PROCESS) return;
    _do_process();
}

// Build the action path prefix for this hand: e.g. "/actions/main/in/left_"
static std::string hand_prefix(OpenVROverlayController3D::Hand hand) {
    return std::string("/actions/main/in/") + (hand == OpenVROverlayController3D::HAND_LEFT ? "left_" : "right_");
}

void OpenVROverlayController3D::_ensure_handles() {
    if (m_handles_ready) return;
    if (!vr::VRInput()) return;

    std::string pfx = hand_prefix(m_hand);

    vr::VRInput()->GetActionSetHandle("/actions/main", &m_action_set);

    vr::VRInput()->GetActionHandle((pfx + "trigger").c_str(),          &m_h_trigger);
    vr::VRInput()->GetActionHandle((pfx + "grip").c_str(),             &m_h_grip);
    vr::VRInput()->GetActionHandle((pfx + "thumbstick").c_str(),       &m_h_thumbstick);
    vr::VRInput()->GetActionHandle((pfx + "trackpad").c_str(),         &m_h_trackpad);
    vr::VRInput()->GetActionHandle((pfx + "trigger_click").c_str(),    &m_h_trigger_click);
    vr::VRInput()->GetActionHandle((pfx + "grip_click").c_str(),       &m_h_grip_click);
    vr::VRInput()->GetActionHandle((pfx + "a").c_str(),                &m_h_a);
    vr::VRInput()->GetActionHandle((pfx + "b").c_str(),                &m_h_b);
    vr::VRInput()->GetActionHandle((pfx + "thumbstick_click").c_str(), &m_h_thumbstick_click);
    vr::VRInput()->GetActionHandle((pfx + "trackpad_click").c_str(),   &m_h_trackpad_click);
    vr::VRInput()->GetActionHandle((pfx + "system").c_str(),           &m_h_system);

    m_handles_ready = (m_action_set != vr::k_ulInvalidActionSetHandle);
}

void OpenVROverlayController3D::_do_process() {
    if (!vr::VRSystem()) {
        m_is_active = false;
        m_has_tracking_data = false;
        return;
    }

    _ensure_handles();

    // --- Pose tracking (legacy GetDeviceToAbsoluteTrackingPose, unchanged) ---
    vr::ETrackedControllerRole role = (m_hand == HAND_LEFT)
        ? vr::TrackedControllerRole_LeftHand
        : vr::TrackedControllerRole_RightHand;

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

    if (device_idx != m_debug_last_device_idx) {
        const char *hand_str = (m_hand == HAND_LEFT) ? "left" : "right";
        if (device_idx == vr::k_unTrackedDeviceIndexInvalid)
            UtilityFunctions::print("OpenVROverlayController3D [", hand_str, "]: no device found");
        else
            UtilityFunctions::print("OpenVROverlayController3D [", hand_str, "]: found device_idx=", (int64_t)device_idx);
        m_debug_last_device_idx = device_idx;
    }

    if (device_idx == vr::k_unTrackedDeviceIndexInvalid) {
        m_is_active = false;
        m_has_tracking_data = false;
        return;
    }
    m_is_active = true;

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

    // --- IVRInput: update action state + read values ---
    if (!m_handles_ready) return;

    vr::VRActiveActionSet_t active{};
    active.ulActionSet = m_action_set;
    vr::VRInput()->UpdateActionState(&active, sizeof(active), 1);

    // Read analog values
    auto read1 = [](vr::VRActionHandle_t h) -> float {
        vr::InputAnalogActionData_t d{};
        vr::VRInput()->GetAnalogActionData(h, &d, sizeof(d), vr::k_ulInvalidInputValueHandle);
        return d.bActive ? d.x : 0.0f;
    };
    auto read2 = [](vr::VRActionHandle_t h) -> Vector2 {
        vr::InputAnalogActionData_t d{};
        vr::VRInput()->GetAnalogActionData(h, &d, sizeof(d), vr::k_ulInvalidInputValueHandle);
        return d.bActive ? Vector2(d.x, d.y) : Vector2();
    };

    m_trigger_val    = read1(m_h_trigger);
    m_grip_val       = read1(m_h_grip);
    m_thumbstick_val = read2(m_h_thumbstick);
    m_trackpad_val   = read2(m_h_trackpad);

    // Read boolean + emit signals on edges
    auto read_bool = [](vr::VRActionHandle_t h) -> bool {
        vr::InputDigitalActionData_t d{};
        vr::VRInput()->GetDigitalActionData(h, &d, sizeof(d), vr::k_ulInvalidInputValueHandle);
        return d.bActive && d.bState;
    };

    struct { ButtonState &state; vr::VRActionHandle_t handle; const char *name; } btns[] = {
        { m_btn_trigger_click,    m_h_trigger_click,    "trigger"   },
        { m_btn_grip_click,       m_h_grip_click,       "grip"      },
        { m_btn_a,                m_h_a,                "a"         },
        { m_btn_b,                m_h_b,                "b"         },
        { m_btn_thumbstick_click, m_h_thumbstick_click, "thumbstick"},
        { m_btn_trackpad_click,   m_h_trackpad_click,   "trackpad"  },
        { m_btn_system,           m_h_system,           "system"    },
    };

    for (auto &e : btns) {
        e.state.prev = e.state.cur;
        e.state.cur  = read_bool(e.handle);
        if  (e.state.cur && !e.state.prev) emit_signal("button_pressed",  String(e.name));
        if (!e.state.cur &&  e.state.prev) emit_signal("button_released", String(e.name));
    }
}

// --- Property accessors ---

void OpenVROverlayController3D::set_hand(Hand p_hand) {
    if (m_hand != p_hand) {
        m_hand = p_hand;
        m_handles_ready = false; // re-resolve handles for the new hand
    }
}
OpenVROverlayController3D::Hand OpenVROverlayController3D::get_hand() const { return m_hand; }
bool OpenVROverlayController3D::get_is_active() const { return m_is_active; }
bool OpenVROverlayController3D::get_has_tracking_data() const { return m_has_tracking_data; }

bool OpenVROverlayController3D::is_button_pressed(const String &p_button) const {
    // Map button names → cached state
    // "menu" is an alias for "b" (B button on Index, closest to a menu button)
    // "touchpad" is an alias for "trackpad"
    if (p_button == "trigger"    || p_button == "trigger_click") return m_btn_trigger_click.cur;
    if (p_button == "grip"       || p_button == "grip_click")    return m_btn_grip_click.cur;
    if (p_button == "a")                                          return m_btn_a.cur;
    if (p_button == "b"          || p_button == "menu")          return m_btn_b.cur;
    if (p_button == "thumbstick" || p_button == "thumbstick_click") return m_btn_thumbstick_click.cur;
    if (p_button == "trackpad"   || p_button == "touchpad"
                                 || p_button == "trackpad_click") return m_btn_trackpad_click.cur;
    if (p_button == "system")                                     return m_btn_system.cur;
    return false;
}

float OpenVROverlayController3D::get_trigger() const {
    return m_trigger_val;
}

Vector2 OpenVROverlayController3D::get_axis(int p_index) const {
    // index 0 = thumbstick (primary joystick), index 1 = trackpad (capacitive surface)
    if (p_index == 0) return m_thumbstick_val;
    if (p_index == 1) return m_trackpad_val;
    return Vector2();
}
