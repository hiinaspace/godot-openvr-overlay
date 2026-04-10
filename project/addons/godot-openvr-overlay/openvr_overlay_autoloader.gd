extends Node

var xr_interface: XRInterfaceOpenVROverlay

func _enter_tree() -> void:
	xr_interface = XRInterfaceOpenVROverlay.new()
	XRServer.add_interface(xr_interface)

func _exit_tree() -> void:
	if xr_interface:
		if xr_interface.is_initialized():
			xr_interface.uninitialize()
		XRServer.remove_interface(xr_interface)
		xr_interface = null
