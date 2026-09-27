## Stylised seated majlis companion built from primitives: thobe / abaya,
## shemagh + agal or hijab, sunglasses, a fanned hand of cards and a gentle
## idle (breathing, looking at whoever is playing).
extends Node3D

const K = preload("res://scripts/mesh_kit.gd")
const Card3D = preload("res://scripts/card3d.gd")

enum Style { SHEMAGH_RED, GHUTRA_WHITE, HIJAB }

var style: int = Style.SHEMAGH_RED
var skin := Color(0.78, 0.57, 0.43)
var head: Node3D
var torso: Node3D
var hand_root: Node3D
var hand_cards: Array = []
var look_target := Vector3.ZERO
var _t := 0.0
var _phase := 0.0


func build(p_style: int, p_skin: Color, phase: float) -> void:
	style = p_style
	skin = p_skin
	_phase = phase
	var female := style == Style.HIJAB
	var cloth := Color(0.1, 0.1, 0.12) if female else Color(0.95, 0.94, 0.92)
	var cloth_mat := K.mat(cloth, 0.85 if not female else 0.6)
	var skin_mat := K.mat(skin, 0.55)

	torso = Node3D.new()
	torso.position = Vector3(0, 0.42, 0)
	add_child(torso)
	# thighs + lap (the table hides most of it)
	for sx in [-1, 1]:
		_limb(self, Vector3(sx * 0.1, 0.44, -0.02), Vector3(sx * 0.12, 0.46, 0.36), 0.085, cloth_mat)
		_limb(self, Vector3(sx * 0.12, 0.44, 0.38), Vector3(sx * 0.13, 0.05, 0.45), 0.065, cloth_mat)
	# torso: elliptic lathe
	var tor := K.lathe(PackedVector2Array([Vector2(0.19, 0.0), Vector2(0.2, 0.12), Vector2(0.19, 0.3), Vector2(0.2, 0.44), Vector2(0.17, 0.52), Vector2(0.08, 0.57), Vector2(0.06, 0.6)]), 24)
	K.add(torso, tor, cloth_mat, Vector3.ZERO, Vector3.ZERO, Vector3(1.0, 1.0, 0.72))
	if not female:
		# thobe collar placket + button
		K.add(torso, K.box(Vector3(0.02, 0.16, 0.01)), K.mat(Color(0.88, 0.87, 0.85), 0.8), Vector3(0, 0.45, 0.14), Vector3(-10, 0, 0))
		K.add(torso, K.sphere(0.008), K.mat(Color(0.8, 0.7, 0.4), 0.3, 0.8), Vector3(0, 0.5, 0.135))
	# neck + head
	K.add(torso, K.cyl(0.045, 0.05, 0.1, 12), skin_mat, Vector3(0, 0.6, 0))
	head = Node3D.new()
	head.position = Vector3(0, 0.72, 0.0)
	torso.add_child(head)
	K.add(head, K.sphere(0.105, 0.23, 24, 14), skin_mat, Vector3.ZERO, Vector3.ZERO, Vector3(0.92, 1.0, 0.98))
	K.add(head, K.sphere(0.016, 0.03, 10, 6), skin_mat, Vector3(0, -0.005, 0.1), Vector3(-10, 0, 0))  # nose
	for sx in [-1, 1]:
		K.add(head, K.sphere(0.022), skin_mat, Vector3(sx * 0.098, -0.005, 0), Vector3.ZERO, Vector3(0.5, 1, 0.8))  # ears
	if female:
		K.add(head, K.capsule(0.007, 0.04), K.mat(Color(0.7, 0.3, 0.3), 0.5), Vector3(0, -0.058, 0.093), Vector3(0, 0, 90))
	else:
		var beard := K.mat(Color(0.13, 0.09, 0.07), 0.9)
		var jaw := PackedVector2Array([Vector2(0.102, -0.005), Vector2(0.101, -0.05), Vector2(0.088, -0.085), Vector2(0.064, -0.112), Vector2(0.032, -0.128), Vector2(0.0, -0.131)])
		K.add(head, K.lathe(jaw, 20, 1.0, PI / 2 - 1.45, PI / 2 + 1.45), beard, Vector3(0, 0, 0.004), Vector3.ZERO, Vector3(0.93, 1.0, 1.0))
		K.add(head, K.capsule(0.009, 0.06), beard, Vector3(0, -0.036, 0.097), Vector3(0, 0, 90))
		K.add(head, K.capsule(0.0075, 0.03), K.mat(Color(0.6, 0.32, 0.3), 0.5), Vector3(0, -0.058, 0.1), Vector3(0, 0, 90))
	_sunglasses()
	match style:
		Style.SHEMAGH_RED:
			_headdress(K.mat(Color.WHITE, 0.9, 0.0, "res://assets/textures/shemagh.png", Vector2(3, 2)), true)
		Style.GHUTRA_WHITE:
			_headdress(K.mat(Color(0.97, 0.97, 0.96), 0.9), true)
		Style.HIJAB:
			_headdress(K.mat(Color(0.08, 0.08, 0.1), 0.55), false)
	# arms reaching forward to hold the cards
	hand_root = Node3D.new()
	hand_root.position = Vector3(0, 0.36, 0.32)
	torso.add_child(hand_root)
	for sx in [-1, 1]:
		var shoulder := Vector3(sx * 0.19, 0.5, 0.0)
		var elbow := Vector3(sx * 0.21, 0.27, 0.11)
		var wrist := Vector3(sx * 0.085, 0.35, 0.3)
		_limb(torso, shoulder, elbow, 0.046, cloth_mat)
		_limb(torso, elbow, wrist, 0.04, cloth_mat)
		K.add(torso, K.sphere(0.036), skin_mat, wrist + Vector3(-sx * 0.02, 0.005, 0.02), Vector3.ZERO, Vector3(0.8, 1.0, 1.1))


func _limb(parent: Node3D, a: Vector3, b: Vector3, r: float, m: Material) -> void:
	var mi := MeshInstance3D.new()
	mi.mesh = K.capsule(r, a.distance_to(b) + r * 2.0)
	mi.material_override = m
	parent.add_child(mi)
	var mid := (a + b) * 0.5
	var dir := (b - a).normalized()
	var up := Vector3.UP if absf(dir.dot(Vector3.UP)) < 0.99 else Vector3.RIGHT
	var basis := Basis.looking_at(dir, up)
	# capsule axis is Y; rotate so +Y follows the limb
	mi.transform = Transform3D(basis * Basis(Vector3.RIGHT, -PI / 2), mid)


func _sunglasses() -> void:
	var lens := K.mat(Color(0.03, 0.03, 0.05), 0.08, 0.6)
	var frame := K.mat(Color(0.02, 0.02, 0.02), 0.3)
	for sx in [-1, 1]:
		K.add(head, K.box(Vector3(0.052, 0.036, 0.012)), lens, Vector3(sx * 0.037, 0.02, 0.1), Vector3(0, sx * 12, 0))
		K.add(head, K.box(Vector3(0.006, 0.006, 0.1)), frame, Vector3(sx * 0.092, 0.028, 0.05))
	K.add(head, K.box(Vector3(0.024, 0.006, 0.006)), frame, Vector3(0, 0.03, 0.106))


func _headdress(m: StandardMaterial3D, agal: bool) -> void:
	m = m.duplicate()
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	var R := 0.118
	var brow := 0.048 if agal else 0.062
	# crown dome from the brow line to the top (full revolution)
	var dome := PackedVector2Array()
	var a0 := asin(brow / R)
	for i in 9:
		var a := lerpf(a0, PI / 2, i / 8.0)
		dome.append(Vector2(R * cos(a), R * sin(a)))
	K.add(head, K.lathe(dome, 24), m, Vector3(0, 0.0, -0.004), Vector3.ZERO, Vector3(0.95, 1.0, 1.0))
	# hood framing the face, open at the front (+Z = PI/2), falling to the shoulders
	var open := 0.95 if agal else 0.78
	var hood := PackedVector2Array([Vector2(0.21, -0.34), Vector2(0.17, -0.21), Vector2(0.135, -0.1), Vector2(0.124, -0.04), Vector2(0.12, 0.01), Vector2(R * cos(a0), brow)])
	K.add(head, K.lathe(hood, 24, 2.0, PI / 2 + open, PI / 2 + TAU - open), m, Vector3(0, 0, -0.004), Vector3.ZERO, Vector3(0.95, 1.0, 1.0))
	if agal:
		var black := K.mat(Color(0.03, 0.03, 0.03), 0.5)
		for i in 2:
			var ring := TorusMesh.new()
			ring.inner_radius = 0.1
			ring.outer_radius = 0.113
			ring.rings = 24
			ring.ring_segments = 8
			K.add(head, ring, black, Vector3(0, 0.078 + i * 0.016, -0.014), Vector3(-10, 0, 0), Vector3(0.95, 1, 1))
	else:
		# hijab wraps under the chin, leaving the face open
		K.add(head, K.lathe(PackedVector2Array([Vector2(0.15, -0.2), Vector2(0.12, -0.13), Vector2(0.1, -0.1), Vector2(0.075, -0.085)]), 24), m, Vector3(0, 0, 0.0), Vector3.ZERO, Vector3(0.95, 1.0, 1.0))


## Show n face-down cards fanned in the hands.
func set_card_count(n: int) -> void:
	while hand_cards.size() > n:
		hand_cards.pop_back().queue_free()
	while hand_cards.size() < n:
		var c := Card3D.new(-1)
		c.scale = Vector3.ONE * 1.1
		hand_root.add_child(c)
		hand_cards.append(c)
	for i in n:
		var t := (i - (n - 1) / 2.0)
		var c: Node3D = hand_cards[i]
		c.transform = Transform3D.IDENTITY
		c.rotate_z(-t * 0.16)
		c.position = Vector3(t * 0.02, 0.07 - absf(t) * 0.004, 0.001 * i)
		c.rotate_object_local(Vector3.RIGHT, -0.25)
		# faces towards the avatar, backs towards the table
		c.rotate_y(PI)


## World-space point where cards leave the hand.
func hand_point() -> Vector3:
	return hand_root.global_position + global_basis.y * 0.04


func _process(delta: float) -> void:
	_t += delta
	var breathe := sin(_t * 1.6 + _phase) * 0.004
	torso.position.y = 0.42 + breathe
	hand_root.rotation.x = sin(_t * 0.7 + _phase) * 0.03
	# smooth head look toward the current target
	var local := head.global_transform.affine_inverse() * look_target
	var yaw := clampf(atan2(local.x, local.z), -0.6, 0.6)
	var pitch := clampf(-atan2(local.y, Vector2(local.x, local.z).length()), -0.35, 0.35)
	head.rotation.y = lerp_angle(head.rotation.y, head.rotation.y + yaw, 1.0 - exp(-delta * 3.0))
	head.rotation.x = lerp_angle(head.rotation.x, head.rotation.x + pitch * 0.5, 1.0 - exp(-delta * 3.0))
	head.rotation.y = clampf(head.rotation.y, -0.7, 0.7)
	head.rotation.x = clampf(head.rotation.x, -0.3, 0.35)
	head.rotation.z = sin(_t * 0.5 + _phase) * 0.03
