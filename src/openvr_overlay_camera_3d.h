#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <openvr.h>

namespace godot {

class OpenVROverlayCamera3D : public Camera3D {
    GDCLASS(OpenVROverlayCamera3D, Camera3D)

public:
    void _ready() override;
    void _notification(int p_what);

    bool get_has_tracking_data() const;

protected:
    static void _bind_methods();

private:
    void _do_process();

    bool m_has_tracking_data = false;
};

} // namespace godot
