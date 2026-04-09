#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "openvr_overlay.h"
#include "openvr_overlay_origin_3d.h"
#include "openvr_overlay_camera_3d.h"
#include "openvr_overlay_controller_3d.h"

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
    GDREGISTER_CLASS(OpenVROverlay);
    GDREGISTER_CLASS(OpenVROverlayOrigin3D);
    GDREGISTER_CLASS(OpenVROverlayCamera3D);
    GDREGISTER_CLASS(OpenVROverlayController3D);
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

extern "C" {
    GDExtensionBool GDE_EXPORT godot_openvr_overlay_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization)
    {
        GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
        init_obj.register_initializer(initialize_gdextension_types);
        init_obj.register_terminator(uninitialize_gdextension_types);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
        return init_obj.init();
    }
}
