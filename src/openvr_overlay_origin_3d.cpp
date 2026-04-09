#include "openvr_overlay_origin_3d.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void OpenVROverlayOrigin3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_world_scale", "scale"), &OpenVROverlayOrigin3D::set_world_scale);
    ClassDB::bind_method(D_METHOD("get_world_scale"), &OpenVROverlayOrigin3D::get_world_scale);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_scale", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater"),
        "set_world_scale", "get_world_scale");
}

void OpenVROverlayOrigin3D::set_world_scale(float p_scale) { m_world_scale = p_scale; }
float OpenVROverlayOrigin3D::get_world_scale() const { return m_world_scale; }
