#include "openvr_overlay_camera_3d.h"
#include "openvr_overlay_origin_3d.h"
#include "openvr_math.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void OpenVROverlayCamera3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_has_tracking_data"), &OpenVROverlayCamera3D::get_has_tracking_data);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_tracking_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY),
        "", "get_has_tracking_data");
}

void OpenVROverlayCamera3D::_process(double /*delta*/) {
    if (!vr::VRSystem()) {
        m_has_tracking_data = false;
        return;
    }

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding, 0.0f,
        poses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t &hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    m_has_tracking_data = hmd.bPoseIsValid;
    if (!m_has_tracking_data)
        return;

    float world_scale = 1.0f;
    if (auto *origin = Object::cast_to<OpenVROverlayOrigin3D>(get_parent()))
        world_scale = origin->get_world_scale();

    Transform3D t = ovr_math::to_godot(hmd.mDeviceToAbsoluteTracking);
    t.origin *= world_scale;
    set_transform(t);
}

bool OpenVROverlayCamera3D::get_has_tracking_data() const { return m_has_tracking_data; }
