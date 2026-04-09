extends Node3D

func _process(delta: float) -> void:
	# Slowly rotate the box so it's easy to verify 3D depth in VR
	$OpenVROverlay/Box.rotate_y(delta * 0.5)
