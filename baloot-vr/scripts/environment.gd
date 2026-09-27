## Builds the sunset majlis: sky, lights, room, furniture, lanterns, the
## engraved Baloot table and the waterfront skyline with mosques.
extends Node3D

const K = preload("res://scripts/mesh_kit.gd")
const S = preload("res://scripts/shaders.gd")
const UI = preload("res://scripts/ui/ui_theme.gd")

const SUN_DIR := Vector3(0.42, 0.06, -1.0)
const TABLE_H := 0.72
const TABLE_R := 0.82

var xr_mode := false
var lantern_lights: Array[OmniLight3D] = []
var _flicker_t := 0.0
var _rng := RandomNumberGenerator.new()


func build(is_xr: bool) -> void:
	xr_mode = is_xr
	_rng.seed = 42
	_world_environment()
	_lights()
	_floor_and_rugs()
	_table()
	_room()
	_majlis_seating()
	_coffee_corner()
	_lanterns()
	_plants()
	_terrace_and_skyline()


func _process(delta: float) -> void:
	_flicker_t += delta
	for i in lantern_lights.size():
		var l := lantern_lights[i]
		l.light_energy = l.get_meta("base") * (0.93 + 0.07 * sin(_flicker_t * (5.0 + i) + i * 1.7) * sin(_flicker_t * 2.3 + i))


# ---------------------------------------------------------------- atmosphere
func _world_environment() -> void:
	var sky_mat := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = S.SKY
	sky_mat.shader = sh
	sky_mat.set_shader_parameter("sun_dir", SUN_DIR)
	var sky := Sky.new()
	sky.sky_material = sky_mat
	sky.radiance_size = Sky.RADIANCE_SIZE_64
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.62, 0.46, 0.42)
	env.ambient_light_energy = 0.55
	env.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.tonemap_exposure = 1.05
	env.tonemap_white = 5.0
	env.fog_enabled = true
	env.fog_light_color = Color(0.78, 0.52, 0.52)
	env.fog_density = 0.0035
	env.fog_sky_affect = 0.0
	env.fog_aerial_perspective = 0.0
	if not xr_mode:
		env.glow_enabled = true
		env.glow_intensity = 0.55
		env.glow_bloom = 0.08
		env.glow_hdr_threshold = 1.1
		env.glow_blend_mode = Environment.GLOW_BLEND_MODE_SOFTLIGHT
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)


func _lights() -> void:
	var sun := DirectionalLight3D.new()
	sun.light_color = Color(1.0, 0.66, 0.42)
	sun.light_energy = 1.1
	sun.shadow_enabled = true
	sun.directional_shadow_mode = DirectionalLight3D.SHADOW_ORTHOGONAL
	sun.directional_shadow_max_distance = 12.0
	sun.shadow_bias = 0.04
	add_child(sun)
	sun.look_at_from_position(Vector3.ZERO, -SUN_DIR.normalized() + Vector3(0, -0.35, 0), Vector3.UP)
	# Warm frontal fill so faces read like the lantern-lit reference.
	var fill := DirectionalLight3D.new()
	fill.light_color = Color(1.0, 0.78, 0.58)
	fill.light_energy = 0.55
	fill.shadow_enabled = false
	fill.sky_mode = DirectionalLight3D.SKY_MODE_LIGHT_ONLY
	add_child(fill)
	fill.look_at_from_position(Vector3(0.3, 2.2, 2.0), Vector3(0, 0.8, -0.5), Vector3.UP)


# ---------------------------------------------------------------- floor & table
func _floor_and_rugs() -> void:
	var stone := K.mat(Color(0.52, 0.4, 0.32), 0.7, 0.0, "res://assets/textures/plaster.png", Vector2(10, 10))
	K.add(self, K.box(Vector3(26, 0.1, 26)), stone, Vector3(0, -0.05, -3))
	var rug := K.mat(Color.WHITE, 0.95, 0.0, "res://assets/textures/carpet_red.png")
	K.add(self, K.box(Vector3(4.6, 0.012, 4.6)), rug, Vector3(0, 0.006, -0.2), Vector3(0, 0, 0))
	var rug2 := K.mat(Color.WHITE, 0.95, 0.0, "res://assets/textures/carpet_navy.png")
	K.add(self, K.box(Vector3(2.6, 0.012, 3.6)), rug2, Vector3(-3.0, 0.004, -2.6), Vector3(0, 8, 0))
	K.add(self, K.box(Vector3(2.4, 0.012, 3.2)), rug2, Vector3(2.9, 0.004, -2.9), Vector3(0, -12, 0))


func _table() -> void:
	var wood := K.mat(Color(0.42, 0.26, 0.15), 0.45, 0.0, "res://assets/textures/wood_dark.png")
	var top := K.mat(Color(1.0, 0.95, 0.9), 0.38, 0.0, "res://assets/textures/table_top.png")
	K.add(self, K.disc(TABLE_R, 96), top, Vector3(0, TABLE_H + 0.0005, 0))
	var rim := K.lathe(PackedVector2Array([Vector2(TABLE_R - 0.04, TABLE_H - 0.07), Vector2(TABLE_R + 0.01, TABLE_H - 0.06), Vector2(TABLE_R + 0.025, TABLE_H - 0.03), Vector2(TABLE_R + 0.02, TABLE_H - 0.005), Vector2(TABLE_R, TABLE_H + 0.0005)]), 96)
	K.add(self, rim, wood, Vector3.ZERO)
	K.add(self, K.cyl(TABLE_R - 0.04, TABLE_R - 0.04, 0.02), wood, Vector3(0, TABLE_H - 0.075, 0))
	var pedestal := K.lathe(PackedVector2Array([Vector2(0.5, 0.0), Vector2(0.48, 0.05), Vector2(0.2, 0.1), Vector2(0.11, 0.2), Vector2(0.09, 0.45), Vector2(0.14, 0.55), Vector2(0.3, 0.62), Vector2(0.55, 0.64)]), 32)
	K.add(self, pedestal, wood, Vector3.ZERO)


# ---------------------------------------------------------------- architecture
func _room() -> void:
	var plaster := K.mat(Color(0.95, 0.82, 0.68), 0.9, 0.0, "res://assets/textures/plaster.png", Vector2(3, 2))
	var dark_wood := K.mat(Color(0.3, 0.18, 0.1), 0.6, 0.0, "res://assets/textures/wood_dark.png", Vector2(4, 1))
	var gold := K.mat(Color(0.85, 0.62, 0.3), 0.35, 0.8)
	# back-left wall with arches (the right side of the room opens onto the terrace)
	K.add(self, K.box(Vector3(6.4, 4.2, 0.4)), plaster, Vector3(-4.0, 2.1, -5.2))
	K.add(self, K.box(Vector3(0.4, 4.2, 9.0)), plaster, Vector3(-6.2, 2.1, -1.0))
	var niche := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = S.ARCH_NICHE
	niche.shader = sh
	for x in [-5.6, -2.5]:
		K.add(self, K.quad(Vector2(1.5, 2.8)), niche, Vector3(x, 1.5, -4.98), Vector3.ZERO, Vector3.ONE, false)
	for z in [-3.0, 0.4]:
		K.add(self, K.quad(Vector2(1.5, 2.8)), niche, Vector3(-5.98, 1.5, z), Vector3(0, 90, 0), Vector3.ONE, false)
	# calligraphy panel on the big pillar behind the left player (like the reference)
	var pillar_pos := Vector3(-1.3, 0, -3.6)
	K.add(self, K.box(Vector3(1.5, 4.2, 0.6)), plaster, pillar_pos + Vector3(0, 2.1, 0))
	K.add(self, K.box(Vector3(1.2, 1.6, 0.04)), K.mat(Color(0.32, 0.2, 0.12), 0.5, 0.0, "res://assets/textures/wood_dark.png"), pillar_pos + Vector3(0, 2.2, 0.31))
	var title := _label3d("البلوت", 190, "logo", Color(0.95, 0.76, 0.46))
	title.position = pillar_pos + Vector3(0, 2.55, 0.335)
	add_child(title)
	var sub := _label3d("أكثر من لعبة… إنها عادة", 78, "arabic", Color(0.93, 0.78, 0.55))
	sub.position = pillar_pos + Vector3(0, 2.0, 0.335)
	add_child(sub)
	# cove light strip on the pillar
	K.add(self, K.box(Vector3(1.3, 0.03, 0.05)), K.unshaded(Color(1.0, 0.72, 0.4)), pillar_pos + Vector3(0, 3.1, 0.33), Vector3.ZERO, Vector3.ONE, false)
	# ceiling with a curved canopy edge towards the view
	K.add(self, K.box(Vector3(12, 0.3, 9.5)), dark_wood, Vector3(-1.6, 4.35, -1.0))
	for i in 7:
		K.add(self, K.box(Vector3(0.16, 0.22, 9.5)), K.mat(Color(0.22, 0.13, 0.07), 0.6), Vector3(-6 + i * 1.6, 4.1, -1.0))
	var cove := K.unshaded(Color(1.0, 0.7, 0.38))
	K.add(self, K.box(Vector3(12, 0.05, 0.06)), cove, Vector3(-1.6, 4.0, -5.7), Vector3.ZERO, Vector3.ONE, false)
	# slender columns holding the canopy on the open side
	for p in [Vector3(4.2, 0, -5.4), Vector3(4.2, 0, -0.6), Vector3(4.2, 0, 3.4), Vector3(0.9, 0, -5.4)]:
		var col := K.lathe(PackedVector2Array([Vector2(0.22, 0), Vector2(0.22, 0.25), Vector2(0.14, 0.32), Vector2(0.12, 3.6), Vector2(0.2, 3.8), Vector2(0.26, 4.2)]), 20)
		K.add(self, col, plaster, p)
		K.add(self, K.cyl(0.125, 0.125, 0.05), gold, p + Vector3(0, 3.55, 0))
	# terrace balustrade along the open sides
	_balustrade(Vector3(1.0, 0, -5.6), Vector3(10.0, 0, -5.6))
	_balustrade(Vector3(4.6, 0, -5.4), Vector3(4.6, 0, 3.4))


func _balustrade(a: Vector3, b: Vector3) -> void:
	var stone := K.mat(Color(0.9, 0.78, 0.64), 0.8)
	var d := b - a
	var n := int(d.length() / 0.28)
	var baluster := K.lathe(PackedVector2Array([Vector2(0.05, 0), Vector2(0.05, 0.08), Vector2(0.03, 0.14), Vector2(0.07, 0.42), Vector2(0.03, 0.66), Vector2(0.05, 0.72)]), 10)
	for i in n:
		K.add(self, baluster, stone, a + d * (float(i) + 0.5) / n)
	var rail := MeshInstance3D.new()
	rail.mesh = K.box(Vector3(0.16, 0.08, d.length()))
	rail.material_override = stone
	add_child(rail)
	rail.look_at_from_position(a + d * 0.5 + Vector3(0, 0.76, 0), b + Vector3(0, 0.76, 0), Vector3.UP)


func _label3d(text: String, px: int, kind: String, color: Color) -> Label3D:
	var l := Label3D.new()
	l.text = text
	l.font = UI.font(kind)
	l.font_size = px
	l.pixel_size = 0.0022
	l.modulate = color
	l.outline_size = 0
	l.shaded = false
	l.double_sided = false
	l.alpha_cut = Label3D.ALPHA_CUT_DISABLED
	return l


# ---------------------------------------------------------------- majlis seating
func _majlis_seating() -> void:
	# seats for the three companions (sofa sections they sit on)
	_sofa(Vector3(-1.45, 0, -0.35), 75.0, 1.3)
	_sofa(Vector3(0, 0, -1.55), 0.0, 1.6)
	_sofa(Vector3(1.45, 0, -0.35), -75.0, 1.3)
	# background majlis
	_sofa(Vector3(-3.4, 0, -4.3), 0.0, 3.2)
	_sofa(Vector3(-5.5, 0, -1.2), 90.0, 4.0)
	_sofa(Vector3(2.6, 0, -4.4), 0.0, 2.6)
	_sofa(Vector3(3.8, 0, -2.3), -90.0, 2.4)
	# low tables between background sofas
	var wood := K.mat(Color(0.4, 0.24, 0.13), 0.5, 0.0, "res://assets/textures/wood.png")
	for p in [Vector3(-3.4, 0, -3.1), Vector3(2.6, 0, -3.3)]:
		K.add(self, K.cyl(0.5, 0.5, 0.05, 32), wood, p + Vector3(0, 0.4, 0))
		K.add(self, K.cyl(0.08, 0.2, 0.38, 12), wood, p + Vector3(0, 0.19, 0))
		_candle_lantern(p + Vector3(0.15, 0.43, 0.05), 0.5)


func _sofa(pos: Vector3, yaw: float, width: float) -> void:
	var root := Node3D.new()
	root.position = pos
	root.rotation_degrees.y = yaw
	add_child(root)
	var fabric := K.mat(Color(0.95, 0.9, 0.86), 0.95, 0.0, "res://assets/textures/kilim.png", Vector2(width, 1))
	var base_col := K.mat(Color(0.3, 0.18, 0.1), 0.7, 0.0, "res://assets/textures/wood_dark.png", Vector2(width, 1))
	var cushion := K.mat(Color(0.55, 0.14, 0.12), 0.9)
	var cushion2 := K.mat(Color(0.2, 0.22, 0.34), 0.9)
	K.add(root, K.box(Vector3(width, 0.18, 0.8)), base_col, Vector3(0, 0.09, 0))
	K.add(root, K.box(Vector3(width - 0.04, 0.16, 0.76)), fabric, Vector3(0, 0.26, 0.0))
	# backrest along -Z of the sofa's local frame
	K.add(root, K.box(Vector3(width, 0.5, 0.2)), fabric, Vector3(0, 0.55, -0.32), Vector3(-8, 0, 0))
	var n := maxi(2, int(width / 0.55))
	for i in n:
		var x := -width / 2 + (i + 0.5) * width / n
		var cm: Material = cushion if i % 2 == 0 else cushion2
		K.add(root, K.sphere(0.2, 0.34, 16, 8), cm, Vector3(x, 0.58, -0.18), Vector3(-15, 0, 0), Vector3(1.1, 1.0, 0.45))
	# arm bolsters
	for sx in [-1, 1]:
		K.add(root, K.cyl(0.11, 0.11, 0.7, 16), fabric, Vector3(sx * (width / 2 - 0.08), 0.44, 0), Vector3(90, 0, 0))


# ---------------------------------------------------------------- coffee corner
func _coffee_corner() -> void:
	var brass := K.mat(Color(0.86, 0.64, 0.32), 0.28, 0.9)
	var wood := K.mat(Color(0.35, 0.2, 0.11), 0.5, 0.0, "res://assets/textures/wood.png")
	var base := Vector3(-1.35, 0, 0.55)
	K.add(self, K.cyl(0.36, 0.36, 0.04, 32), wood, base + Vector3(0, 0.5, 0))
	K.add(self, K.cyl(0.06, 0.16, 0.5, 12), wood, base + Vector3(0, 0.25, 0))
	K.add(self, K.cyl(0.34, 0.33, 0.015, 40), brass, base + Vector3(0, 0.528, 0))
	var tray_top := base + Vector3(0, 0.536, 0)
	# dallah (Arabic coffee pot)
	var dallah := K.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.075, 0.0), Vector2(0.085, 0.02), Vector2(0.08, 0.07), Vector2(0.05, 0.11), Vector2(0.035, 0.15), Vector2(0.045, 0.17), Vector2(0.06, 0.2), Vector2(0.05, 0.23), Vector2(0.02, 0.27), Vector2(0.012, 0.3), Vector2(0.0, 0.31)]), 24)
	var dp := tray_top + Vector3(-0.08, 0, -0.08)
	K.add(self, dallah, brass, dp)
	var spout := MeshInstance3D.new()
	spout.mesh = K.cyl(0.008, 0.018, 0.17, 10)
	spout.material_override = brass
	spout.position = dp + Vector3(0.1, 0.18, 0)
	spout.rotation_degrees = Vector3(0, 0, -55)
	add_child(spout)
	var handle := MeshInstance3D.new()
	var tor := TorusMesh.new()
	tor.inner_radius = 0.05
	tor.outer_radius = 0.062
	handle.mesh = tor
	handle.material_override = brass
	handle.position = dp + Vector3(-0.07, 0.14, 0)
	handle.rotation_degrees = Vector3(90, 0, 0)
	add_child(handle)
	# finjan cups and a bowl of dates
	var cup := K.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.018, 0.0), Vector2(0.03, 0.03), Vector2(0.032, 0.045), Vector2(0.029, 0.045), Vector2(0.0, 0.01)]), 16)
	var porcelain := K.mat(Color(0.96, 0.95, 0.9), 0.3)
	for off in [Vector3(0.12, 0, -0.1), Vector3(0.17, 0, 0.0), Vector3(0.09, 0, 0.1)]:
		K.add(self, cup, porcelain, tray_top + off)
	var bowl := K.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.05, 0.0), Vector2(0.12, 0.04), Vector2(0.14, 0.05), Vector2(0.135, 0.052), Vector2(0.0, 0.02)]), 24)
	var bp := tray_top + Vector3(-0.1, 0, 0.14)
	K.add(self, bowl, brass, bp)
	var date := K.mat(Color(0.3, 0.11, 0.05), 0.35)
	var dm := K.sphere(0.012, 0.03, 10, 6)
	for i in 16:
		var a := i * 2.4
		var r := 0.03 + 0.07 * sqrt(float(i) / 16.0)
		K.add(self, dm, date, bp + Vector3(cos(a) * r, 0.045 + 0.01 * (1.0 - r / 0.1), sin(a) * r), Vector3(90, a * 57.3, 0), Vector3.ONE, false)
	_candle_lantern(base + Vector3(-0.55, 0, -0.1), 1.3)


# ---------------------------------------------------------------- lanterns
func _lanterns() -> void:
	var spots := [Vector3(-1.9, 2.7, -1.9), Vector3(1.5, 2.8, -2.3), Vector3(-0.2, 3.1, -3.7), Vector3(-3.8, 2.6, -0.2), Vector3(-3.9, 2.9, -3.6), Vector3(2.6, 2.6, 0.9), Vector3(-2.6, 2.8, 1.6)]
	for i in spots.size():
		_hanging_lantern(spots[i], i < 4)


func _hanging_lantern(pos: Vector3, with_light: bool) -> void:
	var brass := K.mat(Color(0.7, 0.5, 0.24), 0.35, 0.85)
	var panel := K.mat(Color(0.25, 0.16, 0.08), 0.6, 0.0, "res://assets/textures/lantern_panel.png", Vector2(3, 1), Color(1.0, 0.62, 0.3), 2.4)
	panel.emission_texture = load("res://assets/textures/lantern_panel.png")
	var root := Node3D.new()
	root.position = pos
	add_child(root)
	K.add(root, K.cyl(0.13, 0.13, 0.32, 6), panel, Vector3.ZERO, Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.cyl(0.02, 0.16, 0.14, 6), brass, Vector3(0, 0.23, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.cyl(0.16, 0.05, 0.08, 6), brass, Vector3(0, -0.2, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.sphere(0.03), brass, Vector3(0, 0.32, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.cyl(0.006, 0.006, 4.3 - pos.y - 0.3, 6), brass, Vector3(0, (4.3 - pos.y) / 2 + 0.2, 0), Vector3.ZERO, Vector3.ONE, false)
	_glow_sprite(root, Vector3.ZERO, 0.9, Color(1.0, 0.62, 0.3, 0.55))
	if with_light:
		var l := OmniLight3D.new()
		l.light_color = Color(1.0, 0.64, 0.34)
		l.set_meta("base", 1.6)
		l.light_energy = 1.6
		l.omni_range = 4.2
		l.omni_attenuation = 1.2
		l.shadow_enabled = false
		l.position = Vector3(0, -0.05, 0)
		root.add_child(l)
		lantern_lights.append(l)


func _candle_lantern(pos: Vector3, scale_f: float) -> void:
	var brass := K.mat(Color(0.7, 0.5, 0.24), 0.35, 0.85)
	var panel := K.mat(Color(0.25, 0.16, 0.08), 0.6, 0.0, "res://assets/textures/lantern_panel.png", Vector2(3, 1), Color(1.0, 0.6, 0.28), 2.2)
	panel.emission_texture = load("res://assets/textures/lantern_panel.png")
	var root := Node3D.new()
	root.position = pos
	root.scale = Vector3.ONE * scale_f
	add_child(root)
	K.add(root, K.cyl(0.07, 0.08, 0.02, 8), brass, Vector3(0, 0.01, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.cyl(0.06, 0.06, 0.16, 8), panel, Vector3(0, 0.1, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.cyl(0.0, 0.08, 0.1, 8), brass, Vector3(0, 0.23, 0), Vector3.ZERO, Vector3.ONE, false)
	_glow_sprite(root, Vector3(0, 0.1, 0), 0.45, Color(1.0, 0.6, 0.28, 0.5))


func _glow_sprite(parent: Node3D, pos: Vector3, size: float, color: Color) -> void:
	var s := Sprite3D.new()
	s.texture = load("res://assets/textures/glow.png")
	s.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	s.pixel_size = size / 128.0
	s.modulate = color
	s.shaded = false
	s.transparent = true
	s.no_depth_test = false
	s.position = pos
	s.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(s)
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	mat.albedo_texture = s.texture
	mat.albedo_color = color
	mat.disable_receive_shadows = true
	s.material_override = mat


# ---------------------------------------------------------------- plants
func _plants() -> void:
	_potted_palm(Vector3(-3.0, 0, -4.5), 1.6)
	_potted_palm(Vector3(1.8, 0, -5.0), 1.9)
	_potted_palm(Vector3(-5.4, 0, 2.2), 1.4)
	_potted_palm(Vector3(3.6, 0, 1.9), 1.5)
	# tall palms outside on the terrace edge
	for p in [Vector3(6.5, 0, -7.0), Vector3(9.5, 0, -2.0), Vector3(3.0, 0, -9.0), Vector3(12.0, 0, -8.5), Vector3(8.0, 0, 3.5)]:
		_palm_tree(p, _rng.randf_range(5.5, 8.0))


func _potted_palm(pos: Vector3, h: float) -> void:
	var pot := K.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.18, 0.0), Vector2(0.26, 0.2), Vector2(0.3, 0.42), Vector2(0.27, 0.5), Vector2(0.24, 0.52), Vector2(0.0, 0.48)]), 20)
	K.add(self, pot, K.mat(Color(0.62, 0.36, 0.2), 0.8), pos)
	var leaf := K.mat(Color(0.16, 0.3, 0.12), 0.8)
	leaf.cull_mode = BaseMaterial3D.CULL_DISABLED
	for i in 10:
		var f := MeshInstance3D.new()
		f.mesh = K.frond(h * _rng.randf_range(0.45, 0.65), 0.7, 0.07)
		f.material_override = leaf
		f.position = pos + Vector3(0, 0.45 + _rng.randf() * 0.2, 0)
		f.rotation = Vector3(-_rng.randf_range(0.2, 0.9), i * TAU / 10.0 + _rng.randf() * 0.3, 0)
		add_child(f)


func _palm_tree(pos: Vector3, h: float) -> void:
	var bark := K.mat(Color(0.34, 0.24, 0.16), 0.9)
	var lean := Vector3(_rng.randf_range(-0.3, 0.3), 0, _rng.randf_range(-0.3, 0.3))
	var segs := 8
	for i in segs:
		var t := float(i) / segs
		var p := pos + Vector3(0, h * (t + 0.5 / segs), 0) + lean * t * t * h * 0.3
		K.add(self, K.cyl(0.13 - t * 0.04, 0.15 - t * 0.04, h / segs * 1.05, 10), bark, p)
	var top := pos + Vector3(0, h, 0) + lean * h * 0.3
	var leaf := K.mat(Color(0.1, 0.2, 0.1), 0.8)
	leaf.cull_mode = BaseMaterial3D.CULL_DISABLED
	for i in 12:
		var f := MeshInstance3D.new()
		f.mesh = K.frond(_rng.randf_range(2.4, 3.2), 1.0, 0.28)
		f.material_override = leaf
		f.position = top
		f.rotation = Vector3(-_rng.randf_range(0.0, 0.6), i * TAU / 12.0, 0)
		add_child(f)


# ---------------------------------------------------------------- terrace & skyline
func _terrace_and_skyline() -> void:
	var water_mat := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = S.WATER
	water_mat.shader = sh
	water_mat.set_shader_parameter("sun_dir", SUN_DIR)
	K.add(self, K.quad(Vector2(600, 300)), water_mat, Vector3(40, -0.6, -170), Vector3(-90, 0, 0), Vector3.ONE, false)
	# terrace deck & quay
	K.add(self, K.box(Vector3(30, 0.6, 8)), K.mat(Color(0.6, 0.46, 0.36), 0.8), Vector3(8, -0.3, -10.0))
	# far shore
	K.add(self, K.box(Vector3(600, 1.0, 60)), K.unshaded(Color(0.2, 0.15, 0.25)), Vector3(40, -0.1, -150))
	_city()
	_grand_mosque(Vector3(38, 0.4, -128), 1.0)
	_minaret_pair(Vector3(8, 0.4, -118))
	_grand_mosque(Vector3(-40, 0.4, -140), 0.55)
	# shoreline palms
	for i in 26:
		_palm_silhouette(Vector3(-60 + i * 6.0 + _rng.randf() * 3.0, 0.4, -121 + _rng.randf() * 4.0), _rng.randf_range(5, 9))


func _city() -> void:
	var sh := Shader.new()
	sh.code = S.BUILDING
	for i in 48:
		var x := -120.0 + i * 6.0 + _rng.randf_range(-2, 2)
		if absf(x - 38) < 22:
			continue
		var h := _rng.randf_range(8, 30)
		if _rng.randf() < 0.18:
			h = _rng.randf_range(38, 70)
		var w := _rng.randf_range(3.5, 7)
		var m := ShaderMaterial.new()
		m.shader = sh
		m.set_shader_parameter("seed", float(i))
		m.set_shader_parameter("base_color", Color(0.26, 0.2, 0.36).lerp(Color(0.42, 0.3, 0.42), _rng.randf()))
		var z := -150.0 - _rng.randf() * 20.0
		K.add(self, K.box(Vector3(w, h, w)), m, Vector3(x, h / 2, z), Vector3(0, _rng.randf_range(-10, 10), 0), Vector3.ONE, false)
		if h > 40:
			K.add(self, K.cyl(0.1, 0.3, 8, 6), K.unshaded(Color(0.3, 0.24, 0.38)), Vector3(x, h + 4, z), Vector3.ZERO, Vector3.ONE, false)


func _stone(bright: float) -> StandardMaterial3D:
	return K.unshaded(Color(0.98, 0.8, 0.66) * bright)


func _grand_mosque(pos: Vector3, s: float) -> void:
	var root := Node3D.new()
	root.position = pos
	root.scale = Vector3.ONE * s
	add_child(root)
	var lit := _stone(1.0)
	var shade := _stone(0.72)
	var dome_mat := _stone(1.08)
	K.add(root, K.box(Vector3(46, 7, 18)), shade, Vector3(0, 3.5, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.box(Vector3(46.2, 1.2, 18.2)), lit, Vector3(0, 7.4, 0), Vector3.ZERO, Vector3.ONE, false)
	# lit arcade along the front
	var arch_glow := K.unshaded(Color(1.0, 0.78, 0.46))
	for i in 11:
		K.add(root, K.box(Vector3(1.8, 3.6, 0.2)), arch_glow, Vector3(-20 + i * 4.0, 2.6, 9.05), Vector3.ZERO, Vector3.ONE, false)
	var onion := PackedVector2Array([Vector2(7.0, 0.0), Vector2(7.6, 2.5), Vector2(7.4, 5.0), Vector2(6.2, 7.6), Vector2(4.2, 9.6), Vector2(1.8, 11.2), Vector2(0.5, 12.2), Vector2(0.15, 13.4), Vector2(0.0, 14.0)])
	K.add(root, K.cyl(7.0, 7.0, 4.0, 32), lit, Vector3(0, 10, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.lathe(onion, 32), dome_mat, Vector3(0, 12, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(root, K.cyl(0.25, 0.25, 3.0, 8), K.unshaded(Color(1.0, 0.85, 0.5)), Vector3(0, 27, 0), Vector3.ZERO, Vector3.ONE, false)
	for sx in [-1, 1]:
		K.add(root, K.cyl(3.4, 3.4, 2.0, 24), lit, Vector3(sx * 13, 9, 0), Vector3.ZERO, Vector3.ONE, false)
		K.add(root, K.lathe(_scaled(onion, 0.48), 24), dome_mat, Vector3(sx * 13, 10, 0), Vector3.ZERO, Vector3.ONE, false)
	for p in [Vector3(-24, 0, -8), Vector3(24, 0, -8), Vector3(-24, 0, 8), Vector3(24, 0, 8)]:
		_minaret(root, p, 34.0, lit)


func _scaled(pts: PackedVector2Array, f: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts:
		out.append(p * f)
	return out


func _minaret(parent: Node3D, p: Vector3, h: float, m: Material) -> void:
	K.add(parent, K.cyl(1.0, 1.3, h * 0.55, 12), m, p + Vector3(0, h * 0.275, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(parent, K.cyl(1.6, 1.1, 0.8, 12), m, p + Vector3(0, h * 0.56, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(parent, K.cyl(0.8, 0.95, h * 0.25, 12), m, p + Vector3(0, h * 0.7, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(parent, K.cyl(1.25, 0.8, 0.6, 12), m, p + Vector3(0, h * 0.83, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(parent, K.cyl(0.6, 0.65, h * 0.1, 12), m, p + Vector3(0, h * 0.89, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(parent, K.lathe(PackedVector2Array([Vector2(0.7, 0), Vector2(0.75, 0.6), Vector2(0.4, 1.6), Vector2(0.1, 2.6), Vector2(0.0, 3.2)]), 12), m, p + Vector3(0, h * 0.94, 0), Vector3.ZERO, Vector3.ONE, false)
	K.add(parent, K.box(Vector3(0.6, 1.2, 0.1)), K.unshaded(Color(1.0, 0.8, 0.5)), p + Vector3(0, h * 0.75, 0.9), Vector3.ZERO, Vector3.ONE, false)


func _minaret_pair(pos: Vector3) -> void:
	var root := Node3D.new()
	root.position = pos
	add_child(root)
	var m := _stone(0.95)
	_minaret(root, Vector3(-3, 0, 0), 44.0, m)
	_minaret(root, Vector3(3, 0, 0), 44.0, m)
	K.add(root, K.box(Vector3(12, 8, 6)), _stone(0.7), Vector3(0, 4, 0), Vector3.ZERO, Vector3.ONE, false)


func _palm_silhouette(pos: Vector3, h: float) -> void:
	var m := K.unshaded(Color(0.14, 0.1, 0.18))
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	K.add(self, K.cyl(0.12, 0.2, h, 6), m, pos + Vector3(0, h / 2, 0), Vector3.ZERO, Vector3.ONE, false)
	for i in 8:
		var f := MeshInstance3D.new()
		f.mesh = K.frond(2.4, 1.0, 0.28)
		f.material_override = m
		f.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		f.position = pos + Vector3(0, h, 0)
		f.rotation = Vector3(-0.2, i * TAU / 8.0, 0)
		add_child(f)
