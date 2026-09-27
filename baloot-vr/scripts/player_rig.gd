## The local player: OpenXR headset + Touch controllers with laser pointers,
## or (when no headset is present) a desktop camera driven by the mouse.
## Anything on physics layers 2 (UI) / 4 (cards) carrying a "pointer_target"
## meta receives pointer_move / pointer_button / pointer_exit calls.
extends Node3D

const K = preload("res://scripts/mesh_kit.gd")

const RAY_MASK := 2 | 4
const RAY_LENGTH := 6.0

var xr_active := false
var camera: Camera3D
var origin: XROrigin3D
var controllers: Array = []       # XRController3D or plain Node3D (desktop)
var lasers: Array = []
var dots: Array = []
var _targets := [null, null]
var _active_hand := 1             # last hand that pulled its trigger
var _mouse_pos := Vector2.ZERO
var _yaw := 0.0
var _pitch := -0.32
var _dragging := false
var eye_position := Vector3(0, 1.2, 1.02)
var desktop_look_target := Vector3(0, 0.95, -0.9)


func setup(eye: Vector3) -> bool:
	eye_position = eye
	var xri := XRServer.find_interface("OpenXR")
	if xri and xri.is_initialized():
		xr_active = true
		get_viewport().use_xr = true
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		var rate: float = xri.get_display_refresh_rate() if xri.has_method("get_display_refresh_rate") else 72.0
		var avail: Array = xri.get_available_display_refresh_rates() if xri.has_method("get_available_display_refresh_rates") else []
		if avail.has(90.0):
			xri.set_display_refresh_rate(90.0)
			rate = 90.0
		Engine.physics_ticks_per_second = int(maxf(rate, 72.0))
		_build_xr()
	else:
		_build_desktop()
	return xr_active


func _build_xr() -> void:
	origin = XROrigin3D.new()
	origin.position = eye_position
	add_child(origin)
	camera = XRCamera3D.new()
	camera.near = 0.03
	camera.far = 600.0
	origin.add_child(camera)
	for i in 2:
		var c := XRController3D.new()
		c.tracker = "left_hand" if i == 0 else "right_hand"
		origin.add_child(c)
		_decorate_controller(c, i == 0)
		c.button_pressed.connect(_on_xr_button.bind(i, true))
		c.button_released.connect(_on_xr_button.bind(i, false))
		controllers.append(c)
	# Seated experience: put the headset where the chair is.
	XRServer.center_on_hmd.call_deferred(XRServer.RESET_BUT_KEEP_TILT, true)


func _build_desktop() -> void:
	camera = Camera3D.new()
	camera.fov = 78.0
	camera.near = 0.03
	camera.far = 600.0
	add_child(camera)
	camera.position = eye_position
	camera.look_at(desktop_look_target)
	_yaw = camera.rotation.y
	_pitch = camera.rotation.x
	# Visual controllers held in front of the camera, as in a headset.
	for i in 2:
		var holder := Node3D.new()
		camera.add_child(holder)
		var sx := -1.0 if i == 0 else 1.0
		holder.position = Vector3(sx * 0.2, -0.24, -0.34)
		holder.rotation_degrees = Vector3(38, -sx * 8, sx * 6)
		_decorate_controller(holder, i == 0)
		controllers.append(holder)
	_active_hand = 1


func recenter() -> void:
	if xr_active:
		XRServer.center_on_hmd(XRServer.RESET_BUT_KEEP_TILT, true)
	else:
		camera.look_at(desktop_look_target)
		_yaw = camera.rotation.y
		_pitch = camera.rotation.x


# ---------------------------------------------------------------- visuals
func _decorate_controller(parent: Node3D, left: bool) -> void:
	var white := K.mat(Color(0.93, 0.93, 0.94), 0.35)
	var dark := K.mat(Color(0.12, 0.12, 0.13), 0.4)
	var skin := K.mat(Color(0.8, 0.6, 0.47), 0.6)
	var sleeve := K.mat(Color(0.14, 0.14, 0.16), 0.9)
	var sx := -1.0 if left else 1.0
	var model := Node3D.new()
	parent.add_child(model)
	# face / tracking head
	K.add(model, K.cyl(0.034, 0.036, 0.024, 28), white, Vector3(0, 0.0, 0.0), Vector3.ZERO, Vector3.ONE, false)
	K.add(model, K.cyl(0.03, 0.03, 0.004, 28), dark, Vector3(0, 0.013, 0.0), Vector3.ZERO, Vector3.ONE, false)
	K.add(model, K.cyl(0.006, 0.006, 0.01, 12), dark, Vector3(sx * -0.008, 0.02, -0.004), Vector3.ZERO, Vector3.ONE, false)
	K.add(model, K.sphere(0.0085, 0.01, 12, 6), dark, Vector3(sx * -0.008, 0.026, -0.004), Vector3.ZERO, Vector3.ONE, false)
	for b in 2:
		K.add(model, K.cyl(0.0055, 0.0055, 0.005, 12), K.mat(Color(0.3, 0.3, 0.32), 0.3), Vector3(sx * 0.012, 0.016, 0.004 + b * 0.012), Vector3.ZERO, Vector3.ONE, false)
	# grip
	K.add(model, K.capsule(0.019, 0.12), white, Vector3(0, -0.035, 0.045), Vector3(55, 0, 0), Vector3.ONE, false)
	# a simple hand wrapped around the grip
	var hand := Node3D.new()
	hand.position = Vector3(sx * 0.01, -0.04, 0.06)
	hand.rotation_degrees = Vector3(55, 0, 0)
	model.add_child(hand)
	K.add(hand, K.sphere(0.04, 0.09, 16, 8), skin, Vector3(sx * 0.022, 0.0, 0.012), Vector3.ZERO, Vector3(0.75, 1.0, 0.9), false)
	for f in 4:
		K.add(hand, K.capsule(0.0095, 0.05), skin, Vector3(sx * -0.012, 0.026 - f * 0.019, 0.03), Vector3(0, 0, 90), Vector3.ONE, false)
	K.add(hand, K.capsule(0.011, 0.06), skin, Vector3(sx * 0.0, 0.05, -0.012), Vector3(-20, 0, sx * 20), Vector3.ONE, false)
	K.add(hand, K.capsule(0.042, 0.2), sleeve, Vector3(sx * 0.03, -0.1, 0.02), Vector3(0, 0, 0), Vector3.ONE, false)
	# laser
	var laser := MeshInstance3D.new()
	laser.mesh = K.cyl(0.0012, 0.0012, 1.0, 6)
	var lm := K.unshaded(Color(0.75, 0.88, 1.0, 0.55), "", true)
	lm.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	laser.material_override = lm
	laser.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(laser)
	lasers.append(laser)
	var dot := MeshInstance3D.new()
	dot.mesh = K.sphere(0.006, 0.012, 12, 6)
	dot.material_override = K.unshaded(Color(0.85, 0.93, 1.0))
	dot.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	dot.top_level = true
	add_child(dot)
	dots.append(dot)


# ---------------------------------------------------------------- input
func _on_xr_button(name: String, hand: int, pressed: bool) -> void:
	if name == "trigger_click" or name == "grip_click":
		if pressed:
			_active_hand = hand
		var t = _targets[hand]
		if t and is_instance_valid(t):
			t.pointer_button(dots[hand].global_position, pressed)
	elif pressed and (name == "by_button" or name == "menu_button"):
		recenter()


func _unhandled_input(event: InputEvent) -> void:
	if xr_active:
		return
	if event is InputEventMouseMotion:
		_mouse_pos = event.position
		if _dragging:
			_yaw -= event.relative.x * 0.004
			_pitch = clampf(_pitch - event.relative.y * 0.004, -1.2, 0.9)
			camera.rotation = Vector3(_pitch, _yaw, 0)
	elif event is InputEventMouseButton:
		_mouse_pos = event.position
		if event.button_index == MOUSE_BUTTON_RIGHT:
			_dragging = event.pressed
		elif event.button_index == MOUSE_BUTTON_LEFT:
			var t = _targets[1]
			if t and is_instance_valid(t):
				t.pointer_button(dots[1].global_position, event.pressed)


## Desktop helper for scripted captures: aim the pointer at a world point.
func aim_desktop_at(world: Vector3) -> void:
	if not xr_active:
		_mouse_pos = camera.unproject_position(world)


func _physics_process(_d: float) -> void:
	if camera == null:
		return
	var space := get_world_3d().direct_space_state
	for i in controllers.size():
		var from: Vector3
		var dir: Vector3
		var c: Node3D = controllers[i]
		if xr_active:
			if not (c as XRController3D).get_is_active():
				lasers[i].visible = false
				dots[i].visible = false
				continue
			from = c.global_position
			dir = -c.global_basis.z
		else:
			if i == 0:
				lasers[i].visible = false
				dots[i].visible = false
				continue
			from = camera.project_ray_origin(_mouse_pos)
			dir = camera.project_ray_normal(_mouse_pos)
		var q := PhysicsRayQueryParameters3D.create(from, from + dir * RAY_LENGTH, RAY_MASK)
		q.collide_with_areas = false
		var hit := space.intersect_ray(q)
		var target = null
		var end := from + dir * RAY_LENGTH
		if not hit.is_empty():
			end = hit.position
			var col: Object = hit.collider
			if col.has_meta("pointer_target"):
				target = col.get_meta("pointer_target")
		if target != _targets[i]:
			if _targets[i] and is_instance_valid(_targets[i]):
				_targets[i].pointer_exit()
			_targets[i] = target
		if target:
			target.pointer_move(end)
		# laser from the controller tip to the hit point
		var tip: Vector3 = c.global_position + (-c.global_basis.z) * 0.02
		var laser: MeshInstance3D = lasers[i]
		var len := tip.distance_to(end)
		laser.visible = true
		laser.top_level = true
		var look := (end - tip).normalized()
		var up := Vector3.UP if absf(look.dot(Vector3.UP)) < 0.99 else Vector3.RIGHT
		laser.global_transform = Transform3D(Basis.looking_at(look, up) * Basis(Vector3.RIGHT, -PI / 2), tip + look * len * 0.5)
		laser.scale = Vector3(1, len, 1)
		(laser.material_override as StandardMaterial3D).albedo_color.a = 0.75 if target else 0.35
		dots[i].visible = not hit.is_empty()
		dots[i].global_position = end
