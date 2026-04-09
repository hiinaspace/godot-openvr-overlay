#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <openvr.h>

namespace godot {

class OpenVROverlay : public Node3D {
    GDCLASS(OpenVROverlay, Node3D)

public:
    OpenVROverlay();
    ~OpenVROverlay();

    void _ready() override;
    void _exit_tree() override;
    void _process(double delta) override;

    // Exported properties
    void set_overlay_alpha(float p_alpha);
    float get_overlay_alpha() const;

    void set_viewport_size(const Vector2i &p_size);
    Vector2i get_viewport_size() const;

    void set_near_z(float p_near);
    float get_near_z() const;

    void set_far_z(float p_far);
    float get_far_z() const;

    void set_overlay_key(const String &p_key);
    String get_overlay_key() const;

protected:
    static void _bind_methods();

private:
    void _init_openvr();
    void _shutdown_openvr();
    void _create_viewports();
    void _destroy_viewports();
    void _find_origin(Transform3D &r_xform, float &r_scale) const;
    void _update_eye(vr::EVREye eye,
                     const vr::HmdMatrix34_t &tracking_from_head,
                     SubViewport *viewport,
                     Camera3D *camera);
    void _submit_eye(vr::EVREye eye,
                     vr::VROverlayHandle_t overlay_handle,
                     SubViewport *viewport,
                     const vr::HmdMatrix34_t &tracking_from_head);

    bool m_initialized = false;

    vr::VROverlayHandle_t m_left_overlay = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t m_right_overlay = vr::k_ulOverlayHandleInvalid;

    SubViewport *m_left_viewport = nullptr;
    Camera3D *m_left_camera = nullptr;
    SubViewport *m_right_viewport = nullptr;
    Camera3D *m_right_camera = nullptr;

    // Cached per-eye data from the previous frame (for one-frame-delay submission)
    bool m_has_prev_frame = false;
    vr::HmdMatrix34_t m_prev_tracking_from_head{};
    bool m_logged_projection_diagnostics = false;

    // Exported property values
    float m_overlay_alpha = 1.0f;
    Vector2i m_viewport_size = Vector2i(2048, 2048);
    float m_near_z = 0.05f;
    float m_far_z = 20.0f;
    String m_overlay_key = "";
};

} // namespace godot
