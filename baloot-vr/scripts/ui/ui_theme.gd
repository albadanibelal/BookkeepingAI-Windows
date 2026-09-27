## visionOS-inspired styling: translucent glass, soft white type, pill buttons.
extends RefCounted

const TEXT := Color(1, 1, 1, 0.96)
const TEXT_DIM := Color(1, 1, 1, 0.62)
const GOLD := Color(0.96, 0.8, 0.5)
const GREEN := Color(0.38, 0.9, 0.5)
const RED := Color(1.0, 0.42, 0.42)
const SELECT := Color(0.62, 0.8, 1.0)

static var _cache := {}


static func font(kind: String = "regular") -> Font:
	if _cache.has(kind):
		return _cache[kind]
	var dejavu: FontFile = load("res://assets/fonts/DejaVuSans.ttf")
	var f: FontFile
	match kind:
		"logo":
			f = load("res://assets/fonts/aref-ruqaa-arabic-700-normal.woff2")
			f.fallbacks = [load("res://assets/fonts/aref-ruqaa-latin-700-normal.woff2"), dejavu]
		"bold":
			f = load("res://assets/fonts/inter-latin-600-normal.woff2")
			f.fallbacks = [load("res://assets/fonts/cairo-arabic-700-normal.woff2"), dejavu]
		"medium":
			f = load("res://assets/fonts/inter-latin-500-normal.woff2")
			f.fallbacks = [load("res://assets/fonts/cairo-arabic-600-normal.woff2"), dejavu]
		"arabic":
			f = load("res://assets/fonts/cairo-arabic-700-normal.woff2")
			f.fallbacks = [load("res://assets/fonts/cairo-latin-700-normal.woff2"), dejavu]
		_:
			f = load("res://assets/fonts/inter-latin-400-normal.woff2")
			f.fallbacks = [load("res://assets/fonts/cairo-arabic-400-normal.woff2"), dejavu]
	_cache[kind] = f
	return f


static func glass_box(radius: int, alpha: float = 0.74, border_alpha: float = 0.34) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.3, 0.29, 0.33, alpha)
	sb.set_corner_radius_all(radius)
	sb.set_border_width_all(2)
	sb.border_color = Color(1, 1, 1, border_alpha)
	sb.border_blend = true
	sb.anti_aliasing = true
	sb.anti_aliasing_size = 1.5
	sb.corner_detail = 16
	return sb


static func flat_box(color: Color, radius: int, border: Color = Color(0, 0, 0, 0), border_w: int = 0) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = color
	sb.set_corner_radius_all(radius)
	if border_w > 0:
		sb.set_border_width_all(border_w)
		sb.border_color = border
	sb.anti_aliasing = true
	sb.corner_detail = 12
	return sb


static func label(text: String, size: int, kind: String = "regular", color: Color = TEXT, align := HORIZONTAL_ALIGNMENT_LEFT) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_override("font", font(kind))
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.horizontal_alignment = align
	l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return l


static func icon(name: String, size: int, color: Color = TEXT) -> TextureRect:
	var t := TextureRect.new()
	t.texture = load("res://assets/icons/%s.png" % name)
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	t.custom_minimum_size = Vector2(size, size)
	t.modulate = color
	t.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return t


## Circle holding an icon, like the visionOS sidebar glyph wells.
static func icon_well(name: String, size: int, bg_alpha: float = 0.14) -> PanelContainer:
	var p := PanelContainer.new()
	p.add_theme_stylebox_override("panel", flat_box(Color(1, 1, 1, bg_alpha), size / 2))
	p.custom_minimum_size = Vector2(size, size)
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var c := CenterContainer.new()
	c.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(icon(name, int(size * 0.52)))
	p.add_child(c)
	return p


## Big list-style button: icon well + title + subtitle. Selected state gets the luminous border.
static func menu_button(icon_name: String, title: String, subtitle: String, height: int, width: int) -> Button:
	var b := Button.new()
	b.custom_minimum_size = Vector2(width, height)
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_stylebox_override("normal", flat_box(Color(1, 1, 1, 0.0), height / 3))
	b.add_theme_stylebox_override("hover", flat_box(Color(1, 1, 1, 0.1), height / 3))
	b.add_theme_stylebox_override("pressed", flat_box(Color(1, 1, 1, 0.2), height / 3, Color(SELECT, 0.9), 3))
	b.add_theme_stylebox_override("disabled", flat_box(Color(1, 1, 1, 0.0), height / 3))
	var row := HBoxContainer.new()
	row.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	row.offset_left = height * 0.28
	row.add_theme_constant_override("separation", int(height * 0.3))
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.add_child(icon_well(icon_name, int(height * 0.66)))
	var col := VBoxContainer.new()
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	col.mouse_filter = Control.MOUSE_FILTER_IGNORE
	col.add_theme_constant_override("separation", 0)
	col.add_child(label(title, int(height * 0.3), "medium"))
	if subtitle != "":
		col.add_child(label(subtitle, int(height * 0.21), "regular", TEXT_DIM))
	row.add_child(col)
	b.add_child(row)
	return b


static func set_selected(b: Button, on: bool, height: int) -> void:
	var sb := flat_box(Color(1, 1, 1, 0.2), height / 3, Color(SELECT, 0.95), 3) if on else flat_box(Color(1, 1, 1, 0.0), height / 3)
	if on:
		sb.shadow_color = Color(SELECT, 0.45)
		sb.shadow_size = 14
	b.add_theme_stylebox_override("normal", sb)


## Right-hand action row (Arabic label on the right, icon well on the left like the reference).
static func action_button(icon_name: String, text: String, height: int, width: int) -> Button:
	var b := Button.new()
	b.custom_minimum_size = Vector2(width, height)
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_stylebox_override("normal", flat_box(Color(1, 1, 1, 0.05), 22))
	b.add_theme_stylebox_override("hover", flat_box(Color(1, 1, 1, 0.16), 22, Color(1, 1, 1, 0.35), 2))
	b.add_theme_stylebox_override("pressed", flat_box(Color(1, 1, 1, 0.28), 22, Color(SELECT, 0.9), 3))
	b.add_theme_stylebox_override("disabled", flat_box(Color(1, 1, 1, 0.02), 22))
	var row := HBoxContainer.new()
	row.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	row.offset_left = 14
	row.offset_right = -22
	row.add_theme_constant_override("separation", 18)
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var well := PanelContainer.new()
	well.add_theme_stylebox_override("panel", flat_box(Color(1, 1, 1, 0.12), 18, Color(1, 1, 1, 0.22), 2))
	well.custom_minimum_size = Vector2(height - 24, height - 24)
	well.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	well.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var cc := CenterContainer.new()
	cc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cc.add_child(icon(icon_name, int(height * 0.44)))
	well.add_child(cc)
	row.add_child(well)
	var l := label(text, int(height * 0.36), "arabic", TEXT, HORIZONTAL_ALIGNMENT_RIGHT)
	l.name = "Text"
	l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(l)
	b.add_child(row)
	return b


static func pill_button(icon_name: String, size: int) -> Button:
	var b := Button.new()
	b.custom_minimum_size = Vector2(size, size)
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_stylebox_override("normal", flat_box(Color(1, 1, 1, 0.0), size / 2))
	b.add_theme_stylebox_override("hover", flat_box(Color(1, 1, 1, 0.15), size / 2))
	b.add_theme_stylebox_override("pressed", flat_box(Color(1, 1, 1, 0.3), size / 2))
	var c := CenterContainer.new()
	c.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	c.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var ic := icon(icon_name, int(size * 0.5))
	ic.name = "Icon"
	c.add_child(ic)
	b.add_child(c)
	return b


static func text_button(text: String, size: int, width: int, primary: bool = true) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(width, size)
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_font_override("font", font("arabic"))
	b.add_theme_font_size_override("font_size", int(size * 0.42))
	b.add_theme_color_override("font_color", TEXT)
	b.add_theme_color_override("font_hover_color", Color.WHITE)
	var base := Color(1, 1, 1, 0.22) if primary else Color(1, 1, 1, 0.08)
	b.add_theme_stylebox_override("normal", flat_box(base, size / 2, Color(1, 1, 1, 0.3), 2))
	b.add_theme_stylebox_override("hover", flat_box(base + Color(0, 0, 0, 0.12), size / 2, Color(1, 1, 1, 0.5), 2))
	b.add_theme_stylebox_override("pressed", flat_box(base + Color(0, 0, 0, 0.22), size / 2, Color(SELECT, 0.9), 3))
	return b
