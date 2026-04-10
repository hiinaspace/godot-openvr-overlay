#pragma once

#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/xr_controller_tracker.hpp>
#include <godot_cpp/classes/xr_interface_extension.hpp>
#include <godot_cpp/classes/xr_positional_tracker.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <openvr.h>

namespace godot {

class XRInterfaceOpenVROverlay : public XRInterfaceExtension {
    GDCLASS(XRInterfaceOpenVROverlay, XRInterfaceExtension)

public:
    // --- XRInterfaceExtension virtuals ---
    StringName _get_name() const override;
    uint32_t   _get_capabilities() const override;
    bool       _is_initialized() const override;
    bool       _initialize() override;
    void       _uninitialize() override;
    TrackingStatus _get_tracking_status() const override;
    Vector2    _get_render_target_size() override;
    uint32_t   _get_view_count() override;
    Transform3D _get_camera_transform() override;
    Transform3D _get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) override;
    PackedFloat64Array _get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) override;
    void _process() override;
    void _post_draw_viewport(const RID &p_render_target, const Rect2 &p_screen_rect) override;
    void _end_frame() override;
    void _trigger_haptic_pulse(const String &action_name, const StringName &tracker_name, double frequency, double amplitude, double duration_sec, double delay_sec) override;

    // --- Exported properties ---
    void     set_overlay_alpha(float p_alpha);
    float    get_overlay_alpha() const;
    void     set_near_z(float p_near);
    float    get_near_z() const;
    void     set_far_z(float p_far);
    float    get_far_z() const;
    void     set_overlay_key(const String &p_key);
    String   get_overlay_key() const;

protected:
    static void _bind_methods();

private:
    // --- State ---
    bool m_initialized = false;

    vr::VROverlayHandle_t m_left_overlay  = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t m_right_overlay = vr::k_ulOverlayHandleInvalid;

    // Render-target texture RID (RD-level) saved in _post_draw_viewport
    RID m_texture_rid;

    // Single-layer blit target for the right eye only.
    // Left eye is submitted directly from the array texture (SteamVR always reads layer 0).
    // Right eye needs a blit because SteamVR ignores m_unArrayIndex on SetOverlayTexture.
    RID      m_right_blit_tex;
    uint32_t m_blit_vk_format = 0; // cached VkFormat; recreate if it changes

    // HMD pose cached in _process, used in _end_frame and _get_*_transform
    vr::HmdMatrix34_t m_tracking_from_head{};
    bool m_has_pose = false;

    // Recommended render target size (per eye)
    uint32_t m_rt_width  = 2048;
    uint32_t m_rt_height = 2048;

    // --- Tracker state ---
    Ref<XRControllerTracker> m_left_tracker;
    Ref<XRControllerTracker> m_right_tracker;

    // --- IVRInput handles ---
    bool m_handles_ready = false;
    vr::VRActionSetHandle_t m_action_set = vr::k_ulInvalidActionSetHandle;

    struct HandHandles {
        vr::VRActionHandle_t aim              = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t grip_pose        = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t trigger          = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t grip             = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t thumbstick       = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t trackpad         = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t trigger_click    = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t grip_click       = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t a                = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t b                = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t thumbstick_click = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t trackpad_click   = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t system           = vr::k_ulInvalidActionHandle;
    };
    HandHandles m_left_handles;
    HandHandles m_right_handles;
    vr::VRInputValueHandle_t m_left_source = vr::k_ulInvalidInputValueHandle;
    vr::VRInputValueHandle_t m_right_source = vr::k_ulInvalidInputValueHandle;
    vr::VRActionHandle_t m_haptic_action = vr::k_ulInvalidActionHandle;

    // --- Properties ---
    float  m_overlay_alpha = 1.0f;
    float  m_near_z        = 0.05f;
    float  m_far_z         = 20.0f;
    String m_overlay_key;

    // --- Helpers ---
    void _init_trackers();
    void _shutdown_trackers();
    void _ensure_action_handles();
    void _update_tracker_pose(Ref<XRControllerTracker> &tracker,
                               const HandHandles &handles,
                               vr::VRInputValueHandle_t source_handle,
                               vr::ETrackedControllerRole role,
                               const vr::TrackedDevicePose_t *poses);
    void _update_tracker_inputs(Ref<XRControllerTracker> &tracker,
                                 const HandHandles &handles,
                                 vr::VRInputValueHandle_t source_handle);
    void _ensure_right_blit_texture(RenderingDevice *rd, uint32_t vk_format);
    void _free_blit_textures(RenderingDevice *rd);
    // Submit the original 2-layer array texture (always uses layer 0 — left eye only).
    void _submit_eye_array(vr::EVREye eye,
                           vr::VROverlayHandle_t overlay_handle,
                           uint64_t image, uint32_t vk_format,
                           RenderingDevice *rd);
    // Submit a single-layer blit texture.
    void _submit_eye(vr::EVREye eye,
                     vr::VROverlayHandle_t overlay_handle,
                     uint64_t image, uint32_t vk_format,
                     RenderingDevice *rd);
};

} // namespace godot
