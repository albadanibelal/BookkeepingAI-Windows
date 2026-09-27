## Small procedural modelling kit: materials, primitives, lathe (surface of
## revolution) and a few organic shapes (palm fronds).
extends RefCounted

static var _mats := {}


static func mat(color: Color, rough: float = 0.8, metal: float = 0.0, tex: String = "", uv_scale: Vector2 = Vector2.ONE, emission: Color = Color.BLACK, emission_energy: float = 1.0) -> StandardMaterial3D:
	var key := "%s|%s|%s|%s|%s|%s|%s" % [color, rough, metal, tex, uv_scale, emission, emission_energy]
	if _mats.has(key):
		return _mats[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = rough
	m.metallic = metal
	if tex != "":
		m.albedo_texture = load(tex)
		m.uv1_scale = Vector3(uv_scale.x, uv_scale.y, 1)
		m.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	if emission != Color.BLACK:
		m.emission_enabled = true
		m.emission = emission
		m.emission_energy_multiplier = emission_energy
	_mats[key] = m
	return m


static func unshaded(color: Color, tex: String = "", transparent: bool = false, additive: bool = false) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.albedo_color = color
	if tex != "":
		m.albedo_texture = load(tex)
	if transparent or additive:
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	if additive:
		m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
		m.no_depth_test = false
		m.disable_receive_shadows = true
	return m


static func add(parent: Node, mesh: Mesh, material: Material, pos: Vector3, rot_deg: Vector3 = Vector3.ZERO, scl: Vector3 = Vector3.ONE, shadows: bool = true) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.material_override = material
	mi.position = pos
	mi.rotation_degrees = rot_deg
	mi.scale = scl
	if not shadows:
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mi)
	return mi


static func box(size: Vector3) -> BoxMesh:
	var b := BoxMesh.new()
	b.size = size
	return b


static func cyl(top: float, bottom: float, h: float, seg: int = 24) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = top
	c.bottom_radius = bottom
	c.height = h
	c.radial_segments = seg
	c.rings = 1
	return c


static func sphere(r: float, h: float = -1.0, seg: int = 24, rings: int = 12) -> SphereMesh:
	var s := SphereMesh.new()
	s.radius = r
	s.height = h if h > 0 else r * 2.0
	s.radial_segments = seg
	s.rings = rings
	return s


static func capsule(r: float, h: float) -> CapsuleMesh:
	var c := CapsuleMesh.new()
	c.radius = r
	c.height = h
	c.radial_segments = 16
	c.rings = 6
	return c


static func quad(size: Vector2) -> QuadMesh:
	var q := QuadMesh.new()
	q.size = size
	return q


## Surface of revolution around Y. profile: points (radius, y) from bottom to top.
static func lathe(profile: PackedVector2Array, seg: int = 24, uv_repeat: float = 1.0, a_start: float = 0.0, a_end: float = TAU) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var n := profile.size()
	var length := 0.0
	var acc := [0.0]
	for i in range(1, n):
		length += profile[i].distance_to(profile[i - 1])
		acc.append(length)
	for i in range(n - 1):
		for j in seg:
			var a0 := a_start + (a_end - a_start) * j / seg
			var a1 := a_start + (a_end - a_start) * (j + 1) / seg
			var p00 := Vector3(cos(a0) * profile[i].x, profile[i].y, sin(a0) * profile[i].x)
			var p01 := Vector3(cos(a1) * profile[i].x, profile[i].y, sin(a1) * profile[i].x)
			var p10 := Vector3(cos(a0) * profile[i + 1].x, profile[i + 1].y, sin(a0) * profile[i + 1].x)
			var p11 := Vector3(cos(a1) * profile[i + 1].x, profile[i + 1].y, sin(a1) * profile[i + 1].x)
			var v0: float = acc[i] / max(length, 0.0001)
			var v1: float = acc[i + 1] / max(length, 0.0001)
			var u0 := float(j) / seg * uv_repeat
			var u1 := float(j + 1) / seg * uv_repeat
			st.set_uv(Vector2(u0, 1.0 - v0)); st.add_vertex(p00)
			st.set_uv(Vector2(u1, 1.0 - v0)); st.add_vertex(p01)
			st.set_uv(Vector2(u1, 1.0 - v1)); st.add_vertex(p11)
			st.set_uv(Vector2(u0, 1.0 - v0)); st.add_vertex(p00)
			st.set_uv(Vector2(u1, 1.0 - v1)); st.add_vertex(p11)
			st.set_uv(Vector2(u0, 1.0 - v1)); st.add_vertex(p10)
	st.generate_normals()
	return st.commit()


## Flat disc in XZ facing +Y with planar UVs covering the full texture.
static func disc(radius: float, seg: int = 64) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for j in seg:
		var a0 := TAU * j / seg
		var a1 := TAU * (j + 1) / seg
		var pts := [Vector2.ZERO, Vector2(cos(a0), sin(a0)), Vector2(cos(a1), sin(a1))]
		for p in pts:
			st.set_normal(Vector3.UP)
			st.set_uv(Vector2(p.x * 0.5 + 0.5, p.y * 0.5 + 0.5))
			st.add_vertex(Vector3(p.x * radius, 0, p.y * radius))
	return st.commit()


## Arching palm frond made of leaflet triangles along a curved spine.
static func frond(length: float, droop: float, leaflet: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var steps := 14
	var prev := Vector3.ZERO
	for i in range(1, steps + 1):
		var t := float(i) / steps
		var p := Vector3(0, sin(t * PI * 0.55) * length * 0.35 - droop * t * t * length, t * length)
		var dir := (p - prev).normalized()
		var side := dir.cross(Vector3.UP).normalized()
		var w := leaflet * sin(t * PI) * 1.2 + 0.02
		for s in [-1.0, 1.0]:
			var tip: Vector3 = prev + side * s * w + Vector3(0, -w * 0.5, 0) + dir * w * 0.3
			st.set_normal(Vector3.UP)
			st.add_vertex(prev)
			st.add_vertex(p)
			st.add_vertex(tip)
		prev = p
	return st.commit()
