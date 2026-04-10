@tool
extends EditorPlugin

const AUTOLOAD_NAME := "OpenVROverlayAutoload"
const AUTOLOAD_PATH := "res://addons/godot-openvr-overlay/openvr_overlay_autoloader.gd"

func _enter_tree() -> void:
	if not ProjectSettings.has_setting("autoload/" + AUTOLOAD_NAME):
		add_autoload_singleton(AUTOLOAD_NAME, AUTOLOAD_PATH)

func _disable_plugin() -> void:
	if ProjectSettings.has_setting("autoload/" + AUTOLOAD_NAME):
		remove_autoload_singleton(AUTOLOAD_NAME)
