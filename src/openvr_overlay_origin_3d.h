#pragma once

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class OpenVROverlayOrigin3D : public Node3D {
    GDCLASS(OpenVROverlayOrigin3D, Node3D)

public:
    void set_world_scale(float p_scale);
    float get_world_scale() const;

protected:
    static void _bind_methods();

private:
    float m_world_scale = 1.0f;
};

} // namespace godot
