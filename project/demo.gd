extends Node3D

func _ready() -> void:
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
	$OpenVROverlay/Box.rotate_y(delta * 0.5)
