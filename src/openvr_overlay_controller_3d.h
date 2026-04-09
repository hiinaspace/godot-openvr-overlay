#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <openvr.h>

namespace godot {

class OpenVROverlayController3D : public Node3D {
    GDCLASS(OpenVROverlayController3D, Node3D)

public:
    enum Hand {
        HAND_LEFT  = 0,
        HAND_RIGHT = 1,
    };

    void _ready() override;
    void _notification(int p_what);

    void set_hand(Hand p_hand);
    Hand get_hand() const;

    bool get_is_active() const;
    bool get_has_tracking_data() const;

    bool is_button_pressed(const String &p_button) const;
    float get_trigger() const;
    Vector2 get_axis(int p_index) const;

protected:
    static void _bind_methods();

private:
    void _do_process();
    void _ensure_handles();

    Hand m_hand = HAND_LEFT;
    bool m_is_active = false;
    bool m_has_tracking_data = false;

    // IVRInput handles (initialized lazily once VRInput is available)
    bool m_handles_ready = false;
    vr::VRActionSetHandle_t m_action_set = vr::k_ulInvalidActionSetHandle;

    vr::VRActionHandle_t m_h_trigger       = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_grip          = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_thumbstick    = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_trackpad      = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_trigger_click = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_grip_click    = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_a             = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_b             = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_thumbstick_click = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_trackpad_click   = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_h_system        = vr::k_ulInvalidActionHandle;

    // Cached action state (updated each frame)
    float   m_trigger_val    = 0.0f;
    float   m_grip_val       = 0.0f;
    Vector2 m_thumbstick_val;
    Vector2 m_trackpad_val;

    // Button state for edge detection
    struct ButtonState { bool cur = false; bool prev = false; };
    ButtonState m_btn_trigger_click;
    ButtonState m_btn_grip_click;
    ButtonState m_btn_a;
    ButtonState m_btn_b;
    ButtonState m_btn_thumbstick_click;
    ButtonState m_btn_trackpad_click;
    ButtonState m_btn_system;

    // Pose tracking
    vr::TrackedDeviceIndex_t m_debug_last_device_idx = vr::k_unTrackedDeviceIndexInvalid;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::OpenVROverlayController3D::Hand)
