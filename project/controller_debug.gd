extends OpenVROverlayController3D

@onready var label: Label3D = $Label3D

func _process(_delta: float) -> void:
	# C++ _process runs automatically via _notification; just read the updated state here.
	if not is_active:
		label.text = "(no controller)"
		return

	var trigger_val := get_trigger()
	var pad := get_axis(0)
	var grip := is_button_pressed("grip")
	var menu := is_button_pressed("menu")
	var a_btn := is_button_pressed("a")

	label.text = (
		"trigger: %.2f\n" % trigger_val +
		"pad: (%.2f, %.2f)\n" % [pad.x, pad.y] +
		"grip: %s  menu: %s  a: %s" % [grip, menu, a_btn]
	)

func _on_button_pressed(button_name: String) -> void:
	print(name, " pressed: ", button_name)

func _on_button_released(button_name: String) -> void:
	print(name, " released: ", button_name)

func _ready() -> void:
	button_pressed.connect(_on_button_pressed)
	button_released.connect(_on_button_released)
