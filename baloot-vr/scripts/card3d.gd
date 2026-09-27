## A playing card in the world: front + back quads, optional pointer
## interaction, hover lift and a soft highlight halo.
extends Node3D

const R = preload("res://scripts/rules.gd")

signal hovered(card_node, on)
signal clicked(card_node)

const W := 0.066
const H := 0.0924

static var _front_mats := {}
static var _back_mat: StandardMaterial3D
static var _halo_mat: StandardMaterial3D
static var _quad: QuadMesh
static var _halo_quad: QuadMesh

var card := -1
var front: MeshInstance3D
var back: MeshInstance3D
var halo: MeshInstance3D
var body: StaticBody3D
var interactive := false
var playable := true
var is_hovered := false
var lift := 0.0              # animated hover offset along local +Y
var base_transform := Transform3D.IDENTITY


static func _material(path: String) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_texture = load(path)
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	m.alpha_scissor_threshold = 0.5
	m.roughness = 0.42
	m.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	return m


func _init(c: int = -1) -> void:
	if _quad == null:
		_quad = QuadMesh.new()
		_quad.size = Vector2(W, H)
		_halo_quad = QuadMesh.new()
		_halo_quad.size = Vector2(W * 1.6, H * 1.47)
		_back_mat = _material("res://assets/cards/back.png")
		_halo_mat = StandardMaterial3D.new()
		_halo_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		_halo_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		_halo_mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
		_halo_mat.albedo_texture = load("res://assets/textures/card_glow.png")
		_halo_mat.albedo_color = Color(0.55, 0.78, 1.0, 1.0)
		_halo_mat.disable_receive_shadows = true
	front = MeshInstance3D.new()
	front.mesh = _quad
	add_child(front)
	back = MeshInstance3D.new()
	back.mesh = _quad
	back.material_override = _back_mat
	back.rotation.y = PI
	back.position.z = -0.0004
	add_child(back)
	halo = MeshInstance3D.new()
	halo.mesh = _halo_quad
	halo.material_override = _halo_mat
	halo.position.z = -0.001
	halo.visible = false
	halo.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(halo)
	set_card(c)


func set_card(c: int) -> void:
	card = c
	if c < 0:
		front.material_override = _back_mat
		return
	var id := R.card_id(c)
	if not _front_mats.has(id):
		_front_mats[id] = _material("res://assets/cards/%s.png" % id)
	front.material_override = _front_mats[id]


func set_interactive(on: bool) -> void:
	interactive = on
	if on and body == null:
		body = StaticBody3D.new()
		body.collision_layer = 4
		body.collision_mask = 0
		body.set_meta("pointer_target", self)
		var cs := CollisionShape3D.new()
		var box := BoxShape3D.new()
		box.size = Vector3(W, H, 0.004)
		cs.shape = box
		body.add_child(cs)
		add_child(body)
	if body:
		(body.get_child(0) as CollisionShape3D).disabled = not on
	if not on:
		_set_hover(false)


func set_playable(on: bool) -> void:
	playable = on
	var tint := Color.WHITE if on else Color(0.55, 0.55, 0.58)
	if card >= 0:
		var m: StandardMaterial3D = _front_mats[R.card_id(card)]
		# per-card tint without touching the shared material
		if on:
			front.material_override = m
		else:
			var dim := m.duplicate() as StandardMaterial3D
			dim.albedo_color = tint
			front.material_override = dim


func set_glow(on: bool, color: Color = Color(0.55, 0.78, 1.0, 1.0)) -> void:
	halo.visible = on
	if on:
		var m := _halo_mat.duplicate() as StandardMaterial3D
		m.albedo_color = color
		halo.material_override = m


func _set_hover(on: bool) -> void:
	if is_hovered == on:
		return
	is_hovered = on
	hovered.emit(self, on)
	var tw := create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	tw.tween_property(self, "lift", 0.03 if on else 0.0, 0.15)


func _process(_d: float) -> void:
	if interactive:
		transform = base_transform.translated_local(Vector3(0, lift, lift * 0.3))


func pointer_move(_pos: Vector3) -> void:
	if interactive and playable:
		_set_hover(true)


func pointer_button(_pos: Vector3, pressed: bool) -> void:
	if interactive and playable and pressed:
		clicked.emit(self)


func pointer_exit() -> void:
	_set_hover(false)
