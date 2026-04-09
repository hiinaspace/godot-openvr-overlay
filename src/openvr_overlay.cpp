#include "openvr_overlay.h"
#include "openvr_overlay_origin_3d.h"
#include "openvr_math.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OpenVROverlay::OpenVROverlay() {}
OpenVROverlay::~OpenVROverlay() {}

void OpenVROverlay::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_overlay_alpha", "alpha"), &OpenVROverlay::set_overlay_alpha);
    ClassDB::bind_method(D_METHOD("get_overlay_alpha"), &OpenVROverlay::get_overlay_alpha);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "overlay_alpha", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_overlay_alpha", "get_overlay_alpha");

    ClassDB::bind_method(D_METHOD("set_viewport_size", "size"), &OpenVROverlay::set_viewport_size);
    ClassDB::bind_method(D_METHOD("get_viewport_size"), &OpenVROverlay::get_viewport_size);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "viewport_size"), "set_viewport_size", "get_viewport_size");

    ClassDB::bind_method(D_METHOD("set_near_z", "near_z"), &OpenVROverlay::set_near_z);
    ClassDB::bind_method(D_METHOD("get_near_z"), &OpenVROverlay::get_near_z);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near_z", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater"), "set_near_z", "get_near_z");

    ClassDB::bind_method(D_METHOD("set_far_z", "far_z"), &OpenVROverlay::set_far_z);
    ClassDB::bind_method(D_METHOD("get_far_z"), &OpenVROverlay::get_far_z);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "far_z", PROPERTY_HINT_RANGE, "1,1000,0.1,or_greater"), "set_far_z", "get_far_z");

    ClassDB::bind_method(D_METHOD("set_overlay_key", "key"), &OpenVROverlay::set_overlay_key);
    ClassDB::bind_method(D_METHOD("get_overlay_key"), &OpenVROverlay::get_overlay_key);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "overlay_key"), "set_overlay_key", "get_overlay_key");
}

void OpenVROverlay::_ready() {
    if (Engine::get_singleton()->is_editor_hint())
        return;

    _init_openvr();
    if (m_initialized)
        _create_viewports();
}

void OpenVROverlay::_exit_tree() {
    _destroy_viewports();
    _shutdown_openvr();
}

void OpenVROverlay::_process(double /*delta*/) {
    if (!m_initialized)
        return;

    // Poll overlay events
    vr::VREvent_t event;
    while (vr::VROverlay()->PollNextOverlayEvent(m_left_overlay, &event, sizeof(event))) {
        if (event.eventType == vr::VREvent_Quit) {
            get_tree()->quit();
            return;
        }
    }

    // Get HMD pose
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding, 0.0f,
        poses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t &hmd_pose = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (!hmd_pose.bPoseIsValid)
        return;

    if (!m_logged_projection_diagnostics) {
        const float viewport_aspect = static_cast<float>(m_viewport_size.x) / static_cast<float>(m_viewport_size.y);
        uint32_t rec_width = 0;
        uint32_t rec_height = 0;
        vr::VRSystem()->GetRecommendedRenderTargetSize(&rec_width, &rec_height);
        const float recommended_aspect = rec_height != 0 ? static_cast<float>(rec_width) / static_cast<float>(rec_height) : 0.0f;

        UtilityFunctions::print("OpenVROverlay: using Camera3D::set_frustum fallback",
                                " viewport_size=", m_viewport_size,
                                " viewport_aspect=", viewport_aspect,
                                " recommended_rt=", Vector2i(static_cast<int32_t>(rec_width), static_cast<int32_t>(rec_height)),
                                " recommended_aspect=", recommended_aspect);

        for (vr::EVREye eye : {vr::Eye_Left, vr::Eye_Right}) {
            float l, r, t, b;
            vr::VRSystem()->GetProjectionRaw(eye, &l, &r, &t, &b);

            const float godot_bottom = m_near_z * t;
            const float godot_top = m_near_z * b;
            const float godot_left = m_near_z * l;
            const float godot_right = m_near_z * r;
            const float exact_width = godot_right - godot_left;
            const float exact_height = godot_top - godot_bottom;
            const float exact_aspect = exact_height != 0.0f ? exact_width / exact_height : 0.0f;
            const ovr_math::FrustumApproximation approx =
                ovr_math::approximate_camera_frustum(godot_left, godot_right, godot_bottom, godot_top, viewport_aspect);
            const float left_error = approx.left - godot_left;
            const float right_error = approx.right - godot_right;
            const float width_error = (approx.right - approx.left) - exact_width;
            const float width_error_pct = exact_width != 0.0f ? (width_error / exact_width) * 100.0f : 0.0f;
            const char *eye_name = eye == vr::Eye_Left ? "left" : "right";

            UtilityFunctions::print("  eye=", eye_name,
                                    " exact_aspect=", exact_aspect,
                                    " set_frustum(size=", approx.size,
                                    ", offset=", approx.offset,
                                    ") width_error_pct=", width_error_pct,
                                    " left_error=", left_error,
                                    " right_error=", right_error);
        }

        m_logged_projection_diagnostics = true;
    }

    const vr::HmdMatrix34_t &tracking_from_head = hmd_pose.mDeviceToAbsoluteTracking;

    // Submit previous frame's textures (one-frame delay ensures texture matches its projection)
    if (m_has_prev_frame) {
        _submit_eye(vr::Eye_Left,  m_left_overlay,  m_left_viewport,  m_prev_tracking_from_head);
        _submit_eye(vr::Eye_Right, m_right_overlay, m_right_viewport, m_prev_tracking_from_head);
    }

    // Update cameras with current frame's pose (Godot renders after _process)
    _update_eye(vr::Eye_Left,  tracking_from_head, m_left_viewport,  m_left_camera);
    _update_eye(vr::Eye_Right, tracking_from_head, m_right_viewport, m_right_camera);

    m_prev_tracking_from_head = tracking_from_head;
    m_has_prev_frame = true;
}

// Find the first OpenVROverlayOrigin3D direct child, return its transform + world_scale.
// If none found, returns identity transform and scale 1.
void OpenVROverlay::_find_origin(Transform3D &r_xform, float &r_scale) const {
    r_xform = Transform3D();
    r_scale = 1.0f;
    for (int i = 0; i < get_child_count(); ++i) {
        if (auto *origin = Object::cast_to<OpenVROverlayOrigin3D>(get_child(i))) {
            r_xform = origin->get_global_transform();
            r_scale = origin->get_world_scale();
            return;
        }
    }
}

void OpenVROverlay::_update_eye(vr::EVREye eye,
                                 const vr::HmdMatrix34_t &tracking_from_head,
                                 SubViewport * /*viewport*/,
                                 Camera3D *camera) {
    vr::HmdMatrix34_t head_from_eye = vr::VRSystem()->GetEyeToHeadTransform(eye);
    vr::HmdMatrix34_t tracking_from_eye = ovr_math::multiply(tracking_from_head, head_from_eye);

    // Apply origin transform so moving/scaling OpenVROverlayOrigin3D shifts the rendered world
    Transform3D origin_xform;
    float world_scale;
    _find_origin(origin_xform, world_scale);

    Transform3D eye_pose = ovr_math::to_godot(tracking_from_eye);
    eye_pose.origin *= world_scale;
    camera->set_global_transform(origin_xform * eye_pose);

    // Approximate the OpenVR frustum using Camera3D::set_frustum().
    // This matches exactly when the eye frustum aspect equals the viewport aspect.
    float l, r, t, b;
    vr::VRSystem()->GetProjectionRaw(eye, &l, &r, &t, &b);
    const float godot_left = m_near_z * l;
    const float godot_right = m_near_z * r;
    const float godot_bottom = m_near_z * t;
    const float godot_top = m_near_z * b;
    const float viewport_aspect = static_cast<float>(m_viewport_size.x) / static_cast<float>(m_viewport_size.y);
    const ovr_math::FrustumApproximation approx =
        ovr_math::approximate_camera_frustum(godot_left, godot_right, godot_bottom, godot_top, viewport_aspect);
    camera->set_frustum(approx.size, approx.offset, m_near_z, m_far_z);
}

void OpenVROverlay::_submit_eye(vr::EVREye eye,
                                 vr::VROverlayHandle_t overlay_handle,
                                 SubViewport *viewport,
                                 const vr::HmdMatrix34_t &tracking_from_head) {
    // Extract native OpenGL texture handle.
    // Must use RenderingServer::viewport_get_texture() on the viewport's own RID,
    // not ViewportTexture::get_rid(), which is a proxy and returns 0 from texture_get_native_handle.
    RID vp_rid = viewport->get_viewport_rid();
    RID tex_rid = RenderingServer::get_singleton()->viewport_get_texture(vp_rid);
    uint64_t gl_handle = RenderingServer::get_singleton()->texture_get_native_handle(tex_rid, false);

    if (gl_handle == 0) {
        UtilityFunctions::push_warning("OpenVROverlay: texture_get_native_handle returned 0. Is Compatibility renderer active?");
        return;
    }

    // Submit texture
    vr::Texture_t vr_tex;
    vr_tex.handle = reinterpret_cast<void *>(static_cast<uintptr_t>(gl_handle));
    vr_tex.eType = vr::TextureType_OpenGL;
    vr_tex.eColorSpace = vr::ColorSpace_Gamma;
    vr::VROverlay()->SetOverlayTexture(overlay_handle, &vr_tex);

    // Y-flip texture bounds for OpenGL convention
    vr::VRTextureBounds_t bounds;
    bounds.uMin = 0.0f; bounds.uMax = 1.0f;
    bounds.vMin = 1.0f; bounds.vMax = 0.0f;
    vr::VROverlay()->SetOverlayTextureBounds(overlay_handle, &bounds);

    // Compute eye_from_tracking = inverse(tracking_from_head * head_from_eye)
    vr::HmdMatrix34_t head_from_eye = vr::VRSystem()->GetEyeToHeadTransform(eye);
    vr::HmdMatrix34_t tracking_from_eye = ovr_math::multiply(tracking_from_head, head_from_eye);
    vr::HmdMatrix34_t eye_from_tracking = ovr_math::invert(tracking_from_eye);

    float fl, fr, ft, fb;
    vr::VRSystem()->GetProjectionRaw(eye, &fl, &fr, &ft, &fb);

    vr::VROverlayProjection_t proj;
    proj.fLeft   = fl;
    proj.fRight  = fr;
    proj.fTop    = ft;
    proj.fBottom = fb;

    vr::VROverlay()->SetOverlayTransformProjection(
        overlay_handle,
        vr::TrackingUniverseStanding,
        &eye_from_tracking,
        &proj,
        eye);
}

void OpenVROverlay::_init_openvr() {
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Overlay);

    if (error != vr::VRInitError_None) {
        UtilityFunctions::push_error(String("OpenVROverlay: VR_Init failed: ") +
            vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return;
    }

    if (!vr::VROverlay()) {
        UtilityFunctions::push_error("OpenVROverlay: VROverlay interface unavailable");
        vr::VR_Shutdown();
        return;
    }

    // Determine overlay key: use node name if not set
    String key = m_overlay_key.is_empty() ? String("godot.openvr.") + get_name() : m_overlay_key;
    std::string key_left  = (key + ".left").utf8().get_data();
    std::string key_right = (key + ".right").utf8().get_data();
    std::string name_left  = (String(get_name()) + " Left").utf8().get_data();
    std::string name_right = (String(get_name()) + " Right").utf8().get_data();

    vr::EVROverlayError oe;
    oe = vr::VROverlay()->CreateOverlay(key_left.c_str(), name_left.c_str(), &m_left_overlay);
    if (oe != vr::VROverlayError_None) {
        UtilityFunctions::push_error(String("OpenVROverlay: CreateOverlay (left) failed: ") +
            vr::VROverlay()->GetOverlayErrorNameFromEnum(oe));
        vr::VR_Shutdown();
        return;
    }

    oe = vr::VROverlay()->CreateOverlay(key_right.c_str(), name_right.c_str(), &m_right_overlay);
    if (oe != vr::VROverlayError_None) {
        UtilityFunctions::push_error(String("OpenVROverlay: CreateOverlay (right) failed: ") +
            vr::VROverlay()->GetOverlayErrorNameFromEnum(oe));
        vr::VROverlay()->DestroyOverlay(m_left_overlay);
        m_left_overlay = vr::k_ulOverlayHandleInvalid;
        vr::VR_Shutdown();
        return;
    }

    for (auto handle : {m_left_overlay, m_right_overlay}) {
        vr::VROverlay()->SetOverlayAlpha(handle, m_overlay_alpha);
        vr::VROverlay()->SetOverlayInputMethod(handle, vr::VROverlayInputMethod_None);
        vr::VROverlay()->SetOverlayFlag(handle, vr::VROverlayFlags_NoDashboardTab, true);
        vr::VROverlay()->SetOverlayFlag(handle, vr::VROverlayFlags_SortWithNonSceneOverlays, true);
        vr::VROverlay()->ShowOverlay(handle);
    }

    // Register action manifest for IVRInput (controllers use this for button/axis data)
    String actions_path = ProjectSettings::get_singleton()->globalize_path("res://actions.json");
    std::string actions_path_utf8 = actions_path.utf8().get_data();
    vr::EVRInputError ie = vr::VRInput()->SetActionManifestPath(actions_path_utf8.c_str());
    if (ie != vr::VRInputError_None)
        UtilityFunctions::push_warning(String("OpenVROverlay: SetActionManifestPath failed (") + (int64_t)ie + ") — controller input may not work");
    else
        UtilityFunctions::print("OpenVROverlay: action manifest loaded (", actions_path, ")");

    UtilityFunctions::print("OpenVROverlay: initialized (key=", key, ")");
    m_initialized = true;
}

void OpenVROverlay::_shutdown_openvr() {
    if (!m_initialized)
        return;

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

    vr::VR_Shutdown();
    m_initialized = false;
    m_has_prev_frame = false;
    UtilityFunctions::print("OpenVROverlay: shutdown");
}

void OpenVROverlay::_create_viewports() {
    auto make_eye = [this](const char *vp_name, SubViewport *&vp_out, Camera3D *&cam_out) {
        vp_out = memnew(SubViewport);
        vp_out->set_name(vp_name);
        vp_out->set_size(m_viewport_size);
        vp_out->set_transparent_background(true);
        vp_out->set_update_mode(SubViewport::UPDATE_ALWAYS);
        // Disable default Camera3D that SubViewport would use
        vp_out->set_use_own_world_3d(false);
        add_child(vp_out);

        cam_out = memnew(Camera3D);
        cam_out->set_frustum(1.0f, Vector2(), m_near_z, m_far_z);
        cam_out->set_current(true);
        vp_out->add_child(cam_out);
    };

    make_eye("left_eye",  m_left_viewport,  m_left_camera);
    make_eye("right_eye", m_right_viewport, m_right_camera);
}

void OpenVROverlay::_destroy_viewports() {
    // Viewports and cameras are owned by the scene tree; remove and free them
    if (m_left_viewport && m_left_viewport->is_inside_tree()) {
        m_left_viewport->queue_free();
        m_left_viewport = nullptr;
        m_left_camera = nullptr;
    }
    if (m_right_viewport && m_right_viewport->is_inside_tree()) {
        m_right_viewport->queue_free();
        m_right_viewport = nullptr;
        m_right_camera = nullptr;
    }
}

// --- Property accessors ---

void OpenVROverlay::set_overlay_alpha(float p_alpha) {
    m_overlay_alpha = p_alpha;
    if (m_initialized) {
        vr::VROverlay()->SetOverlayAlpha(m_left_overlay,  m_overlay_alpha);
        vr::VROverlay()->SetOverlayAlpha(m_right_overlay, m_overlay_alpha);
    }
}
float OpenVROverlay::get_overlay_alpha() const { return m_overlay_alpha; }

void OpenVROverlay::set_viewport_size(const Vector2i &p_size) {
    m_viewport_size = p_size;
    if (m_left_viewport)  m_left_viewport->set_size(p_size);
    if (m_right_viewport) m_right_viewport->set_size(p_size);
}
Vector2i OpenVROverlay::get_viewport_size() const { return m_viewport_size; }

void OpenVROverlay::set_near_z(float p_near) { m_near_z = p_near; }
float OpenVROverlay::get_near_z() const { return m_near_z; }

void OpenVROverlay::set_far_z(float p_far) { m_far_z = p_far; }
float OpenVROverlay::get_far_z() const { return m_far_z; }

void OpenVROverlay::set_overlay_key(const String &p_key) { m_overlay_key = p_key; }
String OpenVROverlay::get_overlay_key() const { return m_overlay_key; }
