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

    void _process(double delta) override;

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
    static uint64_t _button_mask(const String &p_button);

    Hand m_hand = HAND_LEFT;
    bool m_is_active = false;
    bool m_has_tracking_data = false;

    vr::VRControllerState_t m_state{};
    uint64_t m_prev_buttons = 0;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::OpenVROverlayController3D::Hand)
