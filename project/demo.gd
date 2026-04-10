extends Node3D

var xr_interface: XRInterface

func _ready() -> void:
	xr_interface = XRServer.find_interface("OpenVR Overlay")
	if xr_interface and (xr_interface.is_initialized() or xr_interface.initialize()):
		print("OpenVR Overlay initialized successfully")

		# Turn off v-sync!
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)

		# Change our main viewport to output to the HMD
		get_viewport().use_xr = true
	else:
		print("OpenVR Overlay not initialized, please check if your headset is connected")

	# Support --quit-after-secs N (time-based, unlike Godot's --quit-after which counts frames).
	# Pass as user args: godot --path project/ -- --quit-after-secs 10
	var args := OS.get_cmdline_user_args()
	for i in range(args.size() - 1):
		if args[i] == "--quit-after-secs":
			var secs := float(args[i + 1])
			get_tree().create_timer(secs).timeout.connect(get_tree().quit)
			break

func _process(delta: float) -> void:
	# Slowly rotate the box so it's easy to verify 3D depth in VR
	$Box.rotate_y(delta * 0.5)
