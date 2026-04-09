#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <openvr.h>

namespace godot {

class OpenVROverlayCamera3D : public Camera3D {
    GDCLASS(OpenVROverlayCamera3D, Camera3D)

public:
    void _process(double delta) override;

    bool get_has_tracking_data() const;

protected:
    static void _bind_methods();

private:
    bool m_has_tracking_data = false;
};

} // namespace godot
