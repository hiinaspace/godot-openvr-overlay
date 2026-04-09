extends OpenVROverlayController3D

@onready var label: Label3D = $Label3D

func _process(_delta: float) -> void:
	# C++ _process runs automatically via _notification; just read the updated state here.
	if not is_active:
		label.text = "(no controller)"
		return

	var trigger_val := get_trigger()
	var stick := get_axis(0)
	var grip := is_button_pressed("grip")
	var a_btn := is_button_pressed("a")
	var b_btn := is_button_pressed("b")

	label.text = (
		"trigger: %.2f\n" % trigger_val +
		"stick: (%.2f, %.2f)\n" % [stick.x, stick.y] +
		"grip: %s  a: %s  b: %s" % [grip, a_btn, b_btn]
	)

func _on_button_pressed(button_name: String) -> void:
	print(name, " pressed: ", button_name)

func _on_button_released(button_name: String) -> void:
	print(name, " released: ", button_name)

func _ready() -> void:
	button_pressed.connect(_on_button_pressed)
	button_released.connect(_on_button_released)
