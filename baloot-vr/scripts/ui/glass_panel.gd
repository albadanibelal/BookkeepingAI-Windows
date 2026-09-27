## A floating visionOS-style window: a 2D Control tree rendered into a
## SubViewport and shown on a quad. Controller / mouse rays are converted to
## mouse events so regular Buttons work in VR.
extends Node3D

const UI = preload("res://scripts/ui/ui_theme.gd")
const PIXELS_PER_METER := 1250.0

var viewport: SubViewport
var content: Control
var quad: MeshInstance3D
var body: StaticBody3D
var px_size := Vector2i(400, 300)
var size_m := Vector2(0.32, 0.24)
var glass: Control
var _pressed := false
var _last_px := Vector2.ZERO
var _fade := 1.0


## width_m: panel width in metres. radius: glass corner radius in px (0 = no glass background).
func setup(px: Vector2i, width_m: float, radius: int = 48, interactive: bool = true, glass_alpha: float = 0.74) -> Control:
	px_size = px
	size_m = Vector2(width_m, width_m * px.y / px.x)
	viewport = SubViewport.new()
	viewport.size = px
	viewport.transparent_bg = true
	viewport.disable_3d = true
	viewport.gui_embed_subwindows = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(viewport)
	content = Control.new()
	content.position = Vector2.ZERO
	content.size = Vector2(px)
	viewport.add_child(content)
	if radius > 0:
		glass = Panel.new()
		glass.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		glass.add_theme_stylebox_override("panel", UI.glass_box(radius, glass_alpha))
		glass.mouse_filter = Control.MOUSE_FILTER_IGNORE
		content.add_child(glass)
		# soft top sheen
		var sheen := Panel.new()
		var sb := UI.flat_box(Color(1, 1, 1, 0.07), radius)
		sb.corner_radius_bottom_left = 0
		sb.corner_radius_bottom_right = 0
		sheen.add_theme_stylebox_override("panel", sb)
		sheen.anchor_right = 1.0
		sheen.anchor_bottom = 0.42
		sheen.offset_left = 2
		sheen.offset_top = 2
		sheen.offset_right = -2
		sheen.mouse_filter = Control.MOUSE_FILTER_IGNORE
		content.add_child(sheen)

	quad = MeshInstance3D.new()
	var qm := QuadMesh.new()
	qm.size = size_m
	quad.mesh = qm
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.blend_mode = BaseMaterial3D.BLEND_MODE_PREMULT_ALPHA
	mat.albedo_texture = viewport.get_texture()
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR
	mat.render_priority = 2
	mat.disable_receive_shadows = true
	quad.material_override = mat
	quad.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(quad)

	if interactive:
		body = StaticBody3D.new()
		body.collision_layer = 2
		body.collision_mask = 0
		body.set_meta("pointer_target", self)
		var cs := CollisionShape3D.new()
		var box := BoxShape3D.new()
		box.size = Vector3(size_m.x, size_m.y, 0.01)
		cs.shape = box
		body.add_child(cs)
		add_child(body)
	return content


## Fade / scale the whole window (used for pop-in and hiding).
func set_fade(v: float) -> void:
	_fade = v
	if quad:
		(quad.material_override as StandardMaterial3D).albedo_color = Color(v, v, v, v)
	visible = v > 0.01
	if body:
		body.process_mode = Node.PROCESS_MODE_INHERIT if v > 0.5 else Node.PROCESS_MODE_DISABLED
		for c in body.get_children():
			(c as CollisionShape3D).disabled = v <= 0.5


func show_animated(on: bool, duration: float = 0.25) -> void:
	var tw := create_tween().set_parallel(true).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	if on:
		scale = Vector3.ONE * 0.92
		tw.tween_property(self, "scale", Vector3.ONE, duration)
	tw.tween_method(set_fade, _fade, 1.0 if on else 0.0, duration)


func face(target: Vector3) -> void:
	look_at(target, Vector3.UP, true)


# ---------------------------------------------------------------- pointer API
func _to_px(world_pos: Vector3) -> Vector2:
	var local := quad.to_local(world_pos)
	var uv := Vector2(local.x / size_m.x + 0.5, 0.5 - local.y / size_m.y)
	return uv * Vector2(px_size)


func pointer_move(world_pos: Vector3) -> void:
	var p := _to_px(world_pos)
	var ev := InputEventMouseMotion.new()
	ev.position = p
	ev.global_position = p
	ev.relative = p - _last_px
	ev.button_mask = MOUSE_BUTTON_MASK_LEFT if _pressed else 0
	_last_px = p
	viewport.push_input(ev)


func pointer_button(world_pos: Vector3, pressed: bool) -> void:
	var p := _to_px(world_pos)
	_pressed = pressed
	var ev := InputEventMouseButton.new()
	ev.position = p
	ev.global_position = p
	ev.button_index = MOUSE_BUTTON_LEFT
	ev.pressed = pressed
	ev.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
	viewport.push_input(ev)


func pointer_exit() -> void:
	var ev := InputEventMouseMotion.new()
	ev.position = Vector2(-100, -100)
	ev.global_position = ev.position
	viewport.push_input(ev)
	if _pressed:
		pointer_button(quad.global_position + Vector3(100, 100, 0), false)
