extends Node3D

@onready var controller: XRController3D = get_parent()

var lastFired: int = 0
var mesh: Mesh
var shape: Shape3D
var fired: bool = false

func _ready():
	mesh = CapsuleMesh.new()
	shape = CapsuleShape3D.new()

	mesh.height = 0.1
	shape.height = 0.1
	mesh.radius = 0.025
	shape.radius = 0.025


func _process(_delta):
	var now = Time.get_ticks_msec()
	if !fired and controller.get_float("trigger") > 0.9 and (now - lastFired) > 100:
		fired = true
		lastFired = now

		var thingRB = RigidBody3D.new()
		var collision = CollisionShape3D.new()
		collision.shape = shape
		thingRB.add_child(collision)
		var meshInstance = MeshInstance3D.new()
		meshInstance.mesh = mesh
		thingRB.add_child(meshInstance)
		add_child(thingRB)
		thingRB.linear_velocity = global_basis * Vector3.FORWARD * 10
		thingRB.top_level = true
		var timer = get_tree().create_timer(3)
		timer.timeout.connect(thingRB.queue_free)

		pass

	if controller.get_float("trigger") < 0.1:
		fired = false
