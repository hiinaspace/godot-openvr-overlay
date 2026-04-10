#include "xr_interface_openvr_overlay.h"
#include "openvr_math.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/xr_controller_tracker.hpp>
#include <godot_cpp/classes/xr_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ============================================================
// Registration
// ============================================================

void XRInterfaceOpenVROverlay::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_overlay_alpha", "alpha"), &XRInterfaceOpenVROverlay::set_overlay_alpha);
    ClassDB::bind_method(D_METHOD("get_overlay_alpha"),          &XRInterfaceOpenVROverlay::get_overlay_alpha);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "overlay_alpha", PROPERTY_HINT_RANGE, "0,1,0.01"),
                 "set_overlay_alpha", "get_overlay_alpha");

    ClassDB::bind_method(D_METHOD("set_near_z", "near_z"), &XRInterfaceOpenVROverlay::set_near_z);
    ClassDB::bind_method(D_METHOD("get_near_z"),           &XRInterfaceOpenVROverlay::get_near_z);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near_z", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater"),
                 "set_near_z", "get_near_z");

    ClassDB::bind_method(D_METHOD("set_far_z", "far_z"), &XRInterfaceOpenVROverlay::set_far_z);
    ClassDB::bind_method(D_METHOD("get_far_z"),          &XRInterfaceOpenVROverlay::get_far_z);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "far_z", PROPERTY_HINT_RANGE, "1,1000,0.1,or_greater"),
                 "set_far_z", "get_far_z");

    ClassDB::bind_method(D_METHOD("set_overlay_key", "key"), &XRInterfaceOpenVROverlay::set_overlay_key);
    ClassDB::bind_method(D_METHOD("get_overlay_key"),        &XRInterfaceOpenVROverlay::get_overlay_key);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "overlay_key"), "set_overlay_key", "get_overlay_key");
}

// ============================================================
// XRInterfaceExtension virtuals
// ============================================================

StringName XRInterfaceOpenVROverlay::_get_name() const {
    return StringName("OpenVR Overlay");
}

uint32_t XRInterfaceOpenVROverlay::_get_capabilities() const {
    return XR_STEREO | XR_VR | XR_EXTERNAL;
}

bool XRInterfaceOpenVROverlay::_is_initialized() const {
    return m_initialized;
}

bool XRInterfaceOpenVROverlay::_initialize() {
    if (m_initialized)
        return true;

    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Overlay);
    if (error != vr::VRInitError_None) {
        UtilityFunctions::push_error(String("OpenVR Overlay: VR_Init failed: ") +
            vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return false;
    }

    if (!vr::VROverlay()) {
        UtilityFunctions::push_error("OpenVR Overlay: VROverlay interface unavailable");
        vr::VR_Shutdown();
        return false;
    }

    // Determine overlay key base
    String base_key = m_overlay_key.is_empty() ? String("godot.openvr.overlay") : m_overlay_key;
    std::string key_left  = (base_key + ".left").utf8().get_data();
    std::string key_right = (base_key + ".right").utf8().get_data();

    vr::EVROverlayError oe;
    oe = vr::VROverlay()->CreateOverlay(key_left.c_str(),  "OpenVR Overlay Left",  &m_left_overlay);
    if (oe != vr::VROverlayError_None) {
        UtilityFunctions::push_error(String("OpenVR Overlay: CreateOverlay (left) failed: ") +
            vr::VROverlay()->GetOverlayErrorNameFromEnum(oe));
        vr::VR_Shutdown();
        return false;
    }
    oe = vr::VROverlay()->CreateOverlay(key_right.c_str(), "OpenVR Overlay Right", &m_right_overlay);
    if (oe != vr::VROverlayError_None) {
        UtilityFunctions::push_error(String("OpenVR Overlay: CreateOverlay (right) failed: ") +
            vr::VROverlay()->GetOverlayErrorNameFromEnum(oe));
        vr::VROverlay()->DestroyOverlay(m_left_overlay);
        m_left_overlay = vr::k_ulOverlayHandleInvalid;
        vr::VR_Shutdown();
        return false;
    }

    for (auto handle : { m_left_overlay, m_right_overlay }) {
        vr::VROverlay()->SetOverlayAlpha(handle, m_overlay_alpha);
        vr::VROverlay()->SetOverlayInputMethod(handle, vr::VROverlayInputMethod_None);
        vr::VROverlay()->SetOverlayFlag(handle, vr::VROverlayFlags_NoDashboardTab, true);
        vr::VROverlay()->SetOverlayFlag(handle, vr::VROverlayFlags_SortWithNonSceneOverlays, true);
        vr::VROverlay()->ShowOverlay(handle);
    }

    // Get recommended render target size
    vr::VRSystem()->GetRecommendedRenderTargetSize(&m_rt_width, &m_rt_height);
    UtilityFunctions::print("OpenVR Overlay: recommended RT size = ",
                            (int64_t)m_rt_width, "x", (int64_t)m_rt_height);

    // Load action manifest from the addon first, then fall back to the old sample-project path.
    String actions_res_path = "res://addons/godot-openvr-overlay/actions.json";
    if (!FileAccess::file_exists(actions_res_path))
        actions_res_path = "res://actions.json";
    String actions_path = ProjectSettings::get_singleton()->globalize_path(actions_res_path);
    std::string actions_utf8 = actions_path.utf8().get_data();
    vr::EVRInputError ie = vr::VRInput()->SetActionManifestPath(actions_utf8.c_str());
    if (ie != vr::VRInputError_None)
        UtilityFunctions::push_warning(String("OpenVR Overlay: SetActionManifestPath failed (") +
            (int64_t)ie + ") — input may not work");
    else
        UtilityFunctions::print("OpenVR Overlay: action manifest loaded (", actions_path, ")");

    // Register with XRServer
    XRServer *xr_server = XRServer::get_singleton();
    if (xr_server)
        xr_server->set_primary_interface(this);

    _init_trackers();

    m_initialized = true;
    UtilityFunctions::print("OpenVR Overlay: initialized (key=", base_key, ")");
    return true;
}

void XRInterfaceOpenVROverlay::_uninitialize() {
    if (!m_initialized)
        return;

    _shutdown_trackers();

    if (m_left_overlay != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(m_left_overlay);
        vr::VROverlay()->DestroyOverlay(m_left_overlay);
        m_left_overlay = vr::k_ulOverlayHandleInvalid;
    }
    if (m_right_overlay != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(m_right_overlay);
        vr::VROverlay()->DestroyOverlay(m_right_overlay);
        m_right_overlay = vr::k_ulOverlayHandleInvalid;
    }

    // Free per-eye blit textures if they were allocated
    {
        RenderingServer *rs = RenderingServer::get_singleton();
        RenderingDevice *rd = rs ? rs->get_rendering_device() : nullptr;
        if (rd) _free_blit_textures(rd);
    }

    vr::VR_Shutdown();
    m_initialized = false;
    m_has_pose = false;
    m_texture_rid = RID();
    m_handles_ready = false;
    m_action_set = vr::k_ulInvalidActionSetHandle;
    m_left_source = vr::k_ulInvalidInputValueHandle;
    m_right_source = vr::k_ulInvalidInputValueHandle;
    m_haptic_action = vr::k_ulInvalidActionHandle;
    m_left_handles = HandHandles{};
    m_right_handles = HandHandles{};
    UtilityFunctions::print("OpenVR Overlay: shutdown");
}

XRInterface::TrackingStatus XRInterfaceOpenVROverlay::_get_tracking_status() const {
    return m_has_pose ? XRInterface::XR_NORMAL_TRACKING : XRInterface::XR_EXCESSIVE_MOTION;
}

Vector2 XRInterfaceOpenVROverlay::_get_render_target_size() {
    return Vector2((float)m_rt_width, (float)m_rt_height);
}

uint32_t XRInterfaceOpenVROverlay::_get_view_count() {
    return 2;
}

Transform3D XRInterfaceOpenVROverlay::_get_camera_transform() {
    if (!m_has_pose)
        return Transform3D();

    XRServer *xr = XRServer::get_singleton();
    double ws = xr ? xr->get_world_scale() : 1.0;

    Transform3D t = ovr_math::to_godot(m_tracking_from_head);
    t.origin *= (float)ws;
    return t;
}

Transform3D XRInterfaceOpenVROverlay::_get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) {
    if (!m_has_pose)
        return p_cam_transform;

    XRServer *xr = XRServer::get_singleton();
    double ws = xr ? xr->get_world_scale() : 1.0;

    vr::EVREye eye = (p_view == 0) ? vr::Eye_Left : vr::Eye_Right;
    vr::HmdMatrix34_t head_from_eye = vr::VRSystem()->GetEyeToHeadTransform(eye);
    Transform3D eye_offset = ovr_math::to_godot(head_from_eye);
    eye_offset.origin *= (float)ws;

    Transform3D hmd_t = ovr_math::to_godot(m_tracking_from_head);
    hmd_t.origin *= (float)ws;

    // p_cam_transform = xr_server->get_world_origin() = XROrigin3D's global transform
    return p_cam_transform * hmd_t * eye_offset;
}

PackedFloat64Array XRInterfaceOpenVROverlay::_get_projection_for_view(
        uint32_t p_view, double /*p_aspect*/, double p_z_near, double p_z_far) {
    PackedFloat64Array arr;
    arr.resize(16);

    vr::EVREye eye = (p_view == 0) ? vr::Eye_Left : vr::Eye_Right;
    vr::HmdMatrix44_t m = vr::VRSystem()->GetProjectionMatrix(eye, (float)p_z_near, (float)p_z_far);

    // OpenVR gives row-major; Godot wants column-major (same memory layout as GL column-major)
    int k = 0;
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            arr[k++] = m.m[row][col];

    return arr;
}

void XRInterfaceOpenVROverlay::_process() {
    if (!m_initialized)
        return;

    // Poll overlay events
    vr::VREvent_t event;
    while (vr::VROverlay()->PollNextOverlayEvent(m_left_overlay, &event, sizeof(event))) {
        if (event.eventType == vr::VREvent_Quit) {
            if (Engine::get_singleton()->get_main_loop())
                Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())->quit();
            return;
        }
    }

    // Get HMD and controller poses
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding, 0.0f,
        poses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t &hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (hmd.bPoseIsValid) {
        m_tracking_from_head = hmd.mDeviceToAbsoluteTracking;
        m_has_pose = true;
    } else {
        m_has_pose = false;
    }

    // Update controller trackers
    _update_tracker_pose(m_left_tracker,  m_left_handles,  m_left_source,  vr::TrackedControllerRole_LeftHand,  poses);
    _update_tracker_pose(m_right_tracker, m_right_handles, m_right_source, vr::TrackedControllerRole_RightHand, poses);

    // Update action inputs
    _ensure_action_handles();
    if (m_handles_ready) {
        vr::VRActiveActionSet_t active{};
        active.ulActionSet = m_action_set;
        vr::VRInput()->UpdateActionState(&active, sizeof(active), 1);

        _update_tracker_inputs(m_left_tracker,  m_left_handles,  m_left_source);
        _update_tracker_inputs(m_right_tracker, m_right_handles, m_right_source);
    }
}

void XRInterfaceOpenVROverlay::_post_draw_viewport(const RID &p_render_target, const Rect2 & /*p_screen_rect*/) {
    // get_render_target_texture returns an RD-level RID suitable for get_driver_resource.
    m_texture_rid = get_render_target_texture(p_render_target);
}

void XRInterfaceOpenVROverlay::_end_frame() {
    if (!m_initialized || !m_has_pose)
        return;
    if (!m_texture_rid.is_valid())
        return;

    RenderingServer *rs = RenderingServer::get_singleton();
    RenderingDevice *rd = rs ? rs->get_rendering_device() : nullptr;
    if (!rd) {
        UtilityFunctions::push_warning("OpenVR Overlay: no RenderingDevice in _end_frame");
        return;
    }

    // get_render_target_texture returns an RD-level RID — works directly with get_driver_resource.
    uint32_t vk_format = (uint32_t)rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT, m_texture_rid, 0);

    if (vk_format == 0) {
        UtilityFunctions::push_warning("OpenVR Overlay: VkFormat is 0 in _end_frame");
        return;
    }

    // Left eye: submit the array texture directly with m_unArrayIndex=0.
    // SteamVR always reads layer 0 from array textures in SetOverlayTexture,
    // so this is correct without any copy.
    uint64_t array_image = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, m_texture_rid, 0);
    if (array_image == 0) {
        UtilityFunctions::push_warning("OpenVR Overlay: array VkImage handle is 0");
        return;
    }
    _submit_eye_array(vr::Eye_Left, m_left_overlay, array_image, vk_format, rd);

    // Right eye: SteamVR ignores m_unArrayIndex so layer 1 can't be submitted directly.
    // Blit layer 1 into a single-layer texture and submit that.
    _ensure_right_blit_texture(rd, vk_format);
    if (!m_right_blit_tex.is_valid())
        return;

    Vector3 origin(0, 0, 0);
    Vector3 size((float)m_rt_width, (float)m_rt_height, 1);
    rd->texture_copy(m_texture_rid, m_right_blit_tex, origin, origin, size, 0, 0, 1, 0);

    uint64_t right_image = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, m_right_blit_tex, 0);
    if (right_image == 0) {
        UtilityFunctions::push_warning("OpenVR Overlay: right blit VkImage handle is 0");
        return;
    }
    _submit_eye(vr::Eye_Right, m_right_overlay, right_image, vk_format, rd);
}

// ============================================================
// Private helpers
// ============================================================

void XRInterfaceOpenVROverlay::_init_trackers() {
    XRServer *xr = XRServer::get_singleton();
    if (!xr) return;

    m_left_tracker.instantiate();
    m_left_tracker->set_tracker_name("left_hand");
    m_left_tracker->set_tracker_desc("Left Hand");
    m_left_tracker->set_tracker_type(XRServer::TRACKER_CONTROLLER);
    m_left_tracker->set_tracker_hand(XRPositionalTracker::TRACKER_HAND_LEFT);
    xr->add_tracker(m_left_tracker);

    m_right_tracker.instantiate();
    m_right_tracker->set_tracker_name("right_hand");
    m_right_tracker->set_tracker_desc("Right Hand");
    m_right_tracker->set_tracker_type(XRServer::TRACKER_CONTROLLER);
    m_right_tracker->set_tracker_hand(XRPositionalTracker::TRACKER_HAND_RIGHT);
    xr->add_tracker(m_right_tracker);
}

void XRInterfaceOpenVROverlay::_shutdown_trackers() {
    XRServer *xr = XRServer::get_singleton();
    if (xr) {
        if (m_left_tracker.is_valid())  xr->remove_tracker(m_left_tracker);
        if (m_right_tracker.is_valid()) xr->remove_tracker(m_right_tracker);
    }
    m_left_tracker.unref();
    m_right_tracker.unref();
}

void XRInterfaceOpenVROverlay::_ensure_action_handles() {
    if (m_handles_ready || !vr::VRInput()) return;

    if (vr::VRInput()->GetActionSetHandle("/actions/main", &m_action_set) != vr::VRInputError_None)
        return;

    auto get = [](const char *path, vr::VRActionHandle_t &h) {
        vr::VRInput()->GetActionHandle(path, &h);
    };
    vr::VRInput()->GetInputSourceHandle("/user/hand/left", &m_left_source);
    vr::VRInput()->GetInputSourceHandle("/user/hand/right", &m_right_source);
    get("/actions/main/out/haptic",               m_haptic_action);

    get("/actions/main/in/aim",                   m_left_handles.aim);
    get("/actions/main/in/grip",                  m_left_handles.grip_pose);
    get("/actions/main/in/left_trigger",          m_left_handles.trigger);
    get("/actions/main/in/left_grip",             m_left_handles.grip);
    get("/actions/main/in/left_thumbstick",       m_left_handles.thumbstick);
    get("/actions/main/in/left_trackpad",         m_left_handles.trackpad);
    get("/actions/main/in/left_trigger_click",    m_left_handles.trigger_click);
    get("/actions/main/in/left_grip_click",       m_left_handles.grip_click);
    get("/actions/main/in/left_a",                m_left_handles.a);
    get("/actions/main/in/left_b",                m_left_handles.b);
    get("/actions/main/in/left_thumbstick_click", m_left_handles.thumbstick_click);
    get("/actions/main/in/left_trackpad_click",   m_left_handles.trackpad_click);
    get("/actions/main/in/left_system",           m_left_handles.system);

    get("/actions/main/in/aim",                    m_right_handles.aim);
    get("/actions/main/in/grip",                   m_right_handles.grip_pose);
    get("/actions/main/in/right_trigger",          m_right_handles.trigger);
    get("/actions/main/in/right_grip",             m_right_handles.grip);
    get("/actions/main/in/right_thumbstick",       m_right_handles.thumbstick);
    get("/actions/main/in/right_trackpad",         m_right_handles.trackpad);
    get("/actions/main/in/right_trigger_click",    m_right_handles.trigger_click);
    get("/actions/main/in/right_grip_click",       m_right_handles.grip_click);
    get("/actions/main/in/right_a",                m_right_handles.a);
    get("/actions/main/in/right_b",                m_right_handles.b);
    get("/actions/main/in/right_thumbstick_click", m_right_handles.thumbstick_click);
    get("/actions/main/in/right_trackpad_click",   m_right_handles.trackpad_click);
    get("/actions/main/in/right_system",           m_right_handles.system);

    m_handles_ready =
        (m_action_set != vr::k_ulInvalidActionSetHandle) &&
        (m_left_source != vr::k_ulInvalidInputValueHandle) &&
        (m_right_source != vr::k_ulInvalidInputValueHandle);
}

void XRInterfaceOpenVROverlay::_trigger_haptic_pulse(
        const String &action_name,
        const StringName &tracker_name,
        double frequency,
        double amplitude,
        double duration_sec,
        double delay_sec) {
    if (!m_initialized || !vr::VRInput())
        return;

    _ensure_action_handles();

    vr::VRActionHandle_t action = m_haptic_action;
    if (action == vr::k_ulInvalidActionHandle) {
        String resolved_action = action_name;
        if (resolved_action == "haptic")
            resolved_action = "/actions/main/out/haptic";
        vr::VRInput()->GetActionHandle(resolved_action.utf8().get_data(), &action);
    }
    if (action == vr::k_ulInvalidActionHandle)
        return;

    vr::VRInputValueHandle_t source = vr::k_ulInvalidInputValueHandle;
    if (tracker_name == StringName("left_hand")) {
        source = m_left_source;
    } else if (tracker_name == StringName("right_hand")) {
        source = m_right_source;
    }

    vr::EVRInputError err = vr::VRInput()->TriggerHapticVibrationAction(
        action,
        (float)delay_sec,
        (float)duration_sec,
        (float)frequency,
        (float)amplitude,
        source);
    if (err != vr::VRInputError_None) {
        UtilityFunctions::push_warning(String("OpenVR Overlay: TriggerHapticVibrationAction failed (") +
            String::num_int64(err) + ")");
    }
}

void XRInterfaceOpenVROverlay::_update_tracker_pose(
        Ref<XRControllerTracker> &tracker,
        const HandHandles &handles,
        vr::VRInputValueHandle_t source_handle,
        vr::ETrackedControllerRole role,
        const vr::TrackedDevicePose_t *poses) {
    if (!tracker.is_valid()) return;

    auto invalidate_all_poses = [&tracker]() {
        tracker->invalidate_pose("default");
        tracker->invalidate_pose("aim");
        tracker->invalidate_pose("grip");
    };

    vr::TrackedDeviceIndex_t idx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(role);
    if (idx == vr::k_unTrackedDeviceIndexInvalid || idx >= vr::k_unMaxTrackedDeviceCount) {
        invalidate_all_poses();
        return;
    }

    const vr::TrackedDevicePose_t &p = poses[idx];
    if (!p.bPoseIsValid) {
        invalidate_all_poses();
        return;
    }

    XRServer *xr = XRServer::get_singleton();
    double ws = xr ? xr->get_world_scale() : 1.0;

    Transform3D t = ovr_math::to_godot(p.mDeviceToAbsoluteTracking);
    t.origin *= (float)ws;

    Vector3 lin_vel(p.vVelocity.v[0], p.vVelocity.v[1], p.vVelocity.v[2]);
    Vector3 ang_vel(p.vAngularVelocity.v[0], p.vAngularVelocity.v[1], p.vAngularVelocity.v[2]);

    XRPose::TrackingConfidence conf =
        (p.eTrackingResult == vr::TrackingResult_Running_OK)
            ? XRPose::XR_TRACKING_CONFIDENCE_HIGH
            : XRPose::XR_TRACKING_CONFIDENCE_LOW;

    tracker->set_pose("default", t, lin_vel, ang_vel, conf);

    auto update_action_pose = [&](const char *name, vr::VRActionHandle_t handle) {
        if (!m_handles_ready || handle == vr::k_ulInvalidActionHandle || source_handle == vr::k_ulInvalidInputValueHandle) {
            tracker->invalidate_pose(name);
            return;
        }

        vr::InputPoseActionData_t data{};
        vr::EVRInputError err = vr::VRInput()->GetPoseActionDataForNextFrame(
            handle,
            vr::TrackingUniverseStanding,
            &data,
            sizeof(data),
            source_handle);
        if (err != vr::VRInputError_None || !data.bActive || !data.pose.bPoseIsValid) {
            tracker->invalidate_pose(name);
            return;
        }

        Transform3D pose_t = ovr_math::to_godot(data.pose.mDeviceToAbsoluteTracking);
        pose_t.origin *= (float)ws;

        Vector3 pose_lin_vel(data.pose.vVelocity.v[0], data.pose.vVelocity.v[1], data.pose.vVelocity.v[2]);
        Vector3 pose_ang_vel(data.pose.vAngularVelocity.v[0], data.pose.vAngularVelocity.v[1], data.pose.vAngularVelocity.v[2]);

        XRPose::TrackingConfidence pose_conf =
            (data.pose.eTrackingResult == vr::TrackingResult_Running_OK)
                ? XRPose::XR_TRACKING_CONFIDENCE_HIGH
                : XRPose::XR_TRACKING_CONFIDENCE_LOW;

        tracker->set_pose(name, pose_t, pose_lin_vel, pose_ang_vel, pose_conf);
    };

    update_action_pose("aim", handles.aim);
    update_action_pose("grip", handles.grip_pose);
}

void XRInterfaceOpenVROverlay::_update_tracker_inputs(
        Ref<XRControllerTracker> &tracker,
        const HandHandles &handles,
        vr::VRInputValueHandle_t source_handle) {
    if (!tracker.is_valid()) return;

    auto f1 = [source_handle](vr::VRActionHandle_t h) -> float {
        vr::InputAnalogActionData_t d{};
        vr::VRInput()->GetAnalogActionData(h, &d, sizeof(d), source_handle);
        return d.bActive ? d.x : 0.0f;
    };
    auto f2 = [source_handle](vr::VRActionHandle_t h) -> Vector2 {
        vr::InputAnalogActionData_t d{};
        vr::VRInput()->GetAnalogActionData(h, &d, sizeof(d), source_handle);
        return d.bActive ? Vector2(d.x, d.y) : Vector2();
    };
    auto fb = [source_handle](vr::VRActionHandle_t h) -> bool {
        vr::InputDigitalActionData_t d{};
        vr::VRInput()->GetDigitalActionData(h, &d, sizeof(d), source_handle);
        return d.bActive && d.bState;
    };

    const float trigger_value = f1(handles.trigger);
    const float grip_value = f1(handles.grip);
    const Vector2 primary_value = f2(handles.thumbstick);
    const Vector2 secondary_value = f2(handles.trackpad);
    const bool trigger_click = fb(handles.trigger_click);
    const bool grip_click = fb(handles.grip_click);
    const bool ax_button = fb(handles.a);
    const bool by_button = fb(handles.b);
    const bool primary_click = fb(handles.thumbstick_click);
    const bool secondary_click = fb(handles.trackpad_click);
    const bool menu_button = fb(handles.system);

    tracker->set_input("trigger",         Variant(trigger_value));
    tracker->set_input("trigger_value",   Variant(trigger_value));
    tracker->set_input("grip",            Variant(grip_value));
    tracker->set_input("grip_value",      Variant(grip_value));
    tracker->set_input("primary",         Variant(primary_value));
    tracker->set_input("secondary",       Variant(secondary_value));
    tracker->set_input("trigger_click",   Variant(trigger_click));
    tracker->set_input("grip_click",      Variant(grip_click));
    tracker->set_input("ax_button",       Variant(ax_button));
    tracker->set_input("button_ax",       Variant(ax_button));
    tracker->set_input("by_button",       Variant(by_button));
    tracker->set_input("button_by",       Variant(by_button));
    tracker->set_input("primary_click",   Variant(primary_click));
    tracker->set_input("secondary_click", Variant(secondary_click));
    tracker->set_input("menu_button",     Variant(menu_button));
}

void XRInterfaceOpenVROverlay::_ensure_right_blit_texture(RenderingDevice *rd, uint32_t vk_format) {
    if (m_right_blit_tex.is_valid() && m_blit_vk_format == vk_format)
        return;

    _free_blit_textures(rd);

    // Godot DataFormat = VkFormat - 1 (Godot's enum starts at 0, VkFormat starts at 1).
    RenderingDevice::DataFormat data_fmt = (RenderingDevice::DataFormat)(vk_format - 1);

    Ref<RDTextureFormat> fmt;
    fmt.instantiate();
    fmt->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
    fmt->set_format(data_fmt);
    fmt->set_width(m_rt_width);
    fmt->set_height(m_rt_height);
    fmt->set_depth(1);
    fmt->set_array_layers(1);
    fmt->set_mipmaps(1);
    fmt->set_usage_bits(
        RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
        RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT);

    Ref<RDTextureView> view;
    view.instantiate();

    TypedArray<PackedByteArray> empty_data;
    m_right_blit_tex = rd->texture_create(fmt, view, empty_data);

    if (!m_right_blit_tex.is_valid()) {
        UtilityFunctions::push_error("OpenVR Overlay: failed to create right blit texture");
        return;
    }

    m_blit_vk_format = vk_format;
    UtilityFunctions::print("OpenVR Overlay: created right blit texture (",
        (int64_t)m_rt_width, "x", (int64_t)m_rt_height,
        " VkFormat=", (int64_t)vk_format, ")");
}

void XRInterfaceOpenVROverlay::_free_blit_textures(RenderingDevice *rd) {
    if (m_right_blit_tex.is_valid()) {
        rd->free_rid(m_right_blit_tex);
        m_right_blit_tex = RID();
    }
    m_blit_vk_format = 0;
}

void XRInterfaceOpenVROverlay::_submit_eye_array(
        vr::EVREye eye,
        vr::VROverlayHandle_t overlay_handle,
        uint64_t image, uint32_t vk_format,
        RenderingDevice *rd) {
    // Submit the 2-layer array texture. SteamVR ignores m_unArrayIndex and always
    // reads layer 0, so this is only correct for the left eye (which IS layer 0).
    vr::VRVulkanTextureArrayData_t vk_data;
    vk_data.m_pDevice           = (VkDevice_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_DEVICE,             RID(), 0);
    vk_data.m_pPhysicalDevice   = (VkPhysicalDevice_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE,   RID(), 0);
    vk_data.m_pInstance         = (VkInstance_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_INSTANCE,         RID(), 0);
    vk_data.m_pQueue            = (VkQueue_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE,            RID(), 0);
    vk_data.m_nQueueFamilyIndex = (uint32_t)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE_FAMILY_INDEX, RID(), 0);
    vk_data.m_nImage            = image;
    vk_data.m_nFormat           = vk_format;
    vk_data.m_nWidth            = m_rt_width;
    vk_data.m_nHeight           = m_rt_height;
    vk_data.m_nSampleCount      = 1;
    vk_data.m_unArraySize       = 2;
    vk_data.m_unArrayIndex      = 0; // SteamVR ignores this; always layer 0

    vr::Texture_t vr_tex = { &vk_data, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
    vr::EVROverlayError oe = vr::VROverlay()->SetOverlayTexture(overlay_handle, &vr_tex);
    if (oe != vr::VROverlayError_None) {
        static bool s_warned = false;
        if (!s_warned) {
            UtilityFunctions::push_warning(
                String("OpenVR Overlay: SetOverlayTexture (array/left) failed (") +
                vr::VROverlay()->GetOverlayErrorNameFromEnum(oe) + ")");
            s_warned = true;
        }
        return;
    }

    // Set projection transform
    vr::HmdMatrix34_t head_from_eye     = vr::VRSystem()->GetEyeToHeadTransform(eye);
    vr::HmdMatrix34_t tracking_from_eye = ovr_math::multiply(m_tracking_from_head, head_from_eye);
    vr::HmdMatrix34_t eye_from_tracking = ovr_math::invert(tracking_from_eye);

    float fl, fr, ft, fbot;
    vr::VRSystem()->GetProjectionRaw(eye, &fl, &fr, &ft, &fbot);
    vr::VROverlayProjection_t proj{ fl, fr, ft, fbot };

    vr::VROverlay()->SetOverlayTransformProjection(
        overlay_handle, vr::TrackingUniverseStanding,
        &eye_from_tracking, &proj, eye);
}

void XRInterfaceOpenVROverlay::_submit_eye(
        vr::EVREye eye,
        vr::VROverlayHandle_t overlay_handle,
        uint64_t image, uint32_t vk_format,
        RenderingDevice *rd) {
    // Single-layer 2D blit texture — use plain VRVulkanTextureData_t.
    vr::VRVulkanTextureData_t vk_data;
    vk_data.m_pDevice           = (VkDevice_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_DEVICE,             RID(), 0);
    vk_data.m_pPhysicalDevice   = (VkPhysicalDevice_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE,   RID(), 0);
    vk_data.m_pInstance         = (VkInstance_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_INSTANCE,         RID(), 0);
    vk_data.m_pQueue            = (VkQueue_T *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE,            RID(), 0);
    vk_data.m_nQueueFamilyIndex = (uint32_t)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE_FAMILY_INDEX, RID(), 0);
    vk_data.m_nImage            = image;
    vk_data.m_nFormat           = vk_format;
    vk_data.m_nWidth            = m_rt_width;
    vk_data.m_nHeight           = m_rt_height;
    vk_data.m_nSampleCount      = 1;

    vr::Texture_t vr_tex = { &vk_data, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
    vr::EVROverlayError oe = vr::VROverlay()->SetOverlayTexture(overlay_handle, &vr_tex);
    if (oe != vr::VROverlayError_None) {
        static bool s_warned = false;
        if (!s_warned) {
            UtilityFunctions::push_warning(
                String("OpenVR Overlay: SetOverlayTexture failed (") +
                vr::VROverlay()->GetOverlayErrorNameFromEnum(oe) + ")");
            s_warned = true;
        }
        return;
    }

    // Set projection transform for this eye
    vr::HmdMatrix34_t head_from_eye     = vr::VRSystem()->GetEyeToHeadTransform(eye);
    vr::HmdMatrix34_t tracking_from_eye = ovr_math::multiply(m_tracking_from_head, head_from_eye);
    vr::HmdMatrix34_t eye_from_tracking = ovr_math::invert(tracking_from_eye);

    float fl, fr, ft, fbot;
    vr::VRSystem()->GetProjectionRaw(eye, &fl, &fr, &ft, &fbot);
    vr::VROverlayProjection_t proj{ fl, fr, ft, fbot };

    vr::VROverlay()->SetOverlayTransformProjection(
        overlay_handle, vr::TrackingUniverseStanding,
        &eye_from_tracking, &proj, eye);
}

// ============================================================
// Property accessors
// ============================================================

void XRInterfaceOpenVROverlay::set_overlay_alpha(float p_alpha) {
    m_overlay_alpha = p_alpha;
    if (m_initialized) {
        vr::VROverlay()->SetOverlayAlpha(m_left_overlay,  m_overlay_alpha);
        vr::VROverlay()->SetOverlayAlpha(m_right_overlay, m_overlay_alpha);
    }
}
float XRInterfaceOpenVROverlay::get_overlay_alpha() const { return m_overlay_alpha; }

void XRInterfaceOpenVROverlay::set_near_z(float p_near) { m_near_z = p_near; }
float XRInterfaceOpenVROverlay::get_near_z() const { return m_near_z; }

void XRInterfaceOpenVROverlay::set_far_z(float p_far) { m_far_z = p_far; }
float XRInterfaceOpenVROverlay::get_far_z() const { return m_far_z; }

void XRInterfaceOpenVROverlay::set_overlay_key(const String &p_key) { m_overlay_key = p_key; }
String XRInterfaceOpenVROverlay::get_overlay_key() const { return m_overlay_key; }
