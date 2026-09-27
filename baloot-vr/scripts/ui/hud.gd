## All floating glass windows around the table.
extends Node3D

const UI = preload("res://scripts/ui/ui_theme.gd")
const Glass = preload("res://scripts/ui/glass_panel.gd")
const R = preload("res://scripts/rules.gd")

signal play_pressed
signal bid_chosen(bid: String, suit: int)
signal next_pressed
signal recenter_pressed
signal speed_toggled(fast: bool)
signal new_game_pressed
signal chat_pressed

const MENU_BTN_H := 104
const MENU_W := 560

var eye := Vector3(0, 1.2, 1.02)
var menu: Glass
var menu_box: VBoxContainer
var menu_buttons := {}
var status: Glass
var clock_label: Label
var profile: Glass
var score: Glass
var score_us: Label
var score_them: Label
var score_mode: Label
var turn_panel: Glass
var turn_title: Label
var turn_sub: Label
var turn_list: VBoxContainer
var count_pill: Glass
var chat_pill: Glass
var mic_icon: TextureRect
var tags := {}
var bubbles := {}
var summary: Glass
var summary_box: VBoxContainer
var fast := false
var _mic_on := true
var _bubble_tokens := {}


func build(p_eye: Vector3, seat_heads: Dictionary) -> void:
	eye = p_eye
	_build_menu()
	_build_status()
	_build_turn_panel()
	_build_small_pills()
	_build_summary()
	for seat in seat_heads:
		_build_tag(seat, seat_heads[seat])
	_clock_tick()
	var t := Timer.new()
	t.wait_time = 15.0
	t.autostart = true
	t.timeout.connect(_clock_tick)
	add_child(t)


func _place(p: Glass, pos: Vector3) -> void:
	p.position = pos
	add_child(p)
	# Windows turn only partly towards the viewer, like visionOS windows arranged around you.
	p.face(eye + Vector3(0, 0, 2.6))


# ---------------------------------------------------------------- main menu
func _build_menu() -> void:
	menu = Glass.new()
	var c := menu.setup(Vector2i(640, 780), 0.46, 56)
	_place(menu, Vector3(-0.64, 1.38, 0.0))
	menu_box = VBoxContainer.new()
	menu_box.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	menu_box.offset_left = 30
	menu_box.offset_right = -30
	menu_box.offset_top = 22
	menu_box.add_theme_constant_override("separation", 6)
	c.add_child(menu_box)
	_show_main_menu()


func _clear(box: Container) -> void:
	for ch in box.get_children():
		box.remove_child(ch)
		ch.queue_free()


func _logo() -> Control:
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", -18)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var spade := UI.icon("spade", 40, Color(0.98, 0.86, 0.66))
	v.add_child(spade)
	var logo := UI.label("بلوت", 118, "logo", Color(0.99, 0.9, 0.74), HORIZONTAL_ALIGNMENT_CENTER)
	logo.add_theme_color_override("font_shadow_color", Color(0.3, 0.18, 0.05, 0.6))
	logo.add_theme_constant_override("shadow_offset_y", 4)
	v.add_child(logo)
	var sub := UI.label("B  A  L  O  O  T", 20, "medium", Color(1, 1, 1, 0.8), HORIZONTAL_ALIGNMENT_CENTER)
	v.add_child(sub)
	v.custom_minimum_size = Vector2(0, 230)
	return v


func _show_main_menu() -> void:
	_clear(menu_box)
	menu_box.add_child(_logo())
	var items := [["play", "spade", "Play", "Classic Baloot"], ["tournament", "trophy", "Tournament", "Compete & Climb"],
		["friends", "people", "Friends", "Private Rooms"], ["settings", "gear", "Settings", "Game Settings"]]
	menu_buttons.clear()
	for i in items.size():
		var it: Array = items[i]
		var b := UI.menu_button(it[1], it[2], it[3], MENU_BTN_H, MENU_W)
		menu_box.add_child(b)
		menu_buttons[it[0]] = b
		b.pressed.connect(_on_menu.bind(it[0]))
		if i < items.size() - 1:
			var sep := ColorRect.new()
			sep.color = Color(1, 1, 1, 0.1)
			sep.custom_minimum_size = Vector2(0, 2)
			sep.mouse_filter = Control.MOUSE_FILTER_IGNORE
			menu_box.add_child(sep)
	UI.set_selected(menu_buttons["play"], true, MENU_BTN_H)


func _show_settings() -> void:
	_clear(menu_box)
	var head := HBoxContainer.new()
	var back := UI.pill_button("home", 70)
	back.pressed.connect(_show_main_menu)
	head.add_child(back)
	head.add_child(UI.label("  Settings", 40, "bold"))
	menu_box.add_child(head)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, 20)
	menu_box.add_child(gap)
	var speed := UI.menu_button("sun", "Game speed", "Fast" if fast else "Normal", MENU_BTN_H, MENU_W)
	speed.pressed.connect(func():
		fast = not fast
		speed_toggled.emit(fast)
		_show_settings())
	menu_box.add_child(speed)
	var rec := UI.menu_button("recenter", "Recenter view", "Or press B / Y", MENU_BTN_H, MENU_W)
	rec.pressed.connect(func(): recenter_pressed.emit())
	menu_box.add_child(rec)
	var ng := UI.menu_button("spade", "New game", "Restart the scores", MENU_BTN_H, MENU_W)
	ng.pressed.connect(func():
		new_game_pressed.emit()
		_show_main_menu())
	menu_box.add_child(ng)
	menu_box.add_child(UI.label("Offline vs. bots  •  v1.0", 22, "regular", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER))


func _on_menu(id: String) -> void:
	match id:
		"play":
			play_pressed.emit()
		"settings":
			_show_settings()
		"tournament", "friends":
			show_bubble(0, "قريباً — Coming soon", 2.2)


# ---------------------------------------------------------------- status & profile & score
func _pill_container(glass: Glass, px: Vector2i, width: float, pos: Vector3, radius: int) -> HBoxContainer:
	var c := glass.setup(px, width, radius, false)
	_place(glass, pos)
	var h := HBoxContainer.new()
	h.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	h.offset_left = 30
	h.offset_right = -30
	h.alignment = BoxContainer.ALIGNMENT_CENTER
	h.add_theme_constant_override("separation", 22)
	c.add_child(h)
	return h


func _build_status() -> void:
	status = Glass.new()
	var h := _pill_container(status, Vector2i(420, 96), 0.25, Vector3(0.58, 1.86, -0.3), 48)
	h.add_child(UI.icon("wifi", 40))
	clock_label = UI.label("8:24 PM", 36, "medium", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	clock_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(clock_label)
	h.add_child(UI.icon("battery", 44))

	profile = Glass.new()
	var p := _pill_container(profile, Vector2i(520, 130), 0.3, Vector3(0.62, 1.75, -0.25), 65)
	var av := TextureRect.new()
	av.texture = load("res://assets/icons/avatar_belal.png")
	av.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	av.custom_minimum_size = Vector2(92, 92)
	av.mouse_filter = Control.MOUSE_FILTER_IGNORE
	p.add_child(av)
	var col := VBoxContainer.new()
	col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_theme_constant_override("separation", -4)
	col.add_child(UI.label("Belal", 40, "bold"))
	var st := HBoxContainer.new()
	var dot := Panel.new()
	dot.custom_minimum_size = Vector2(14, 14)
	dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	dot.add_theme_stylebox_override("panel", UI.flat_box(UI.GREEN, 7))
	st.add_child(dot)
	st.add_child(UI.label(" In Game", 26, "regular", UI.TEXT_DIM))
	col.add_child(st)
	p.add_child(col)
	p.add_child(UI.icon("profile", 44))

	score = Glass.new()
	var s := _pill_container(score, Vector2i(520, 150), 0.3, Vector3(0.65, 1.615, -0.2), 48)
	s.add_theme_constant_override("separation", 10)
	var them := VBoxContainer.new()
	them.add_theme_constant_override("separation", -8)
	them.add_child(UI.label("لهم", 26, "arabic", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER))
	score_them = UI.label("0", 52, "bold", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	them.add_child(score_them)
	them.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var mid := VBoxContainer.new()
	mid.add_theme_constant_override("separation", -6)
	score_mode = UI.label("—", 30, "arabic", UI.GOLD, HORIZONTAL_ALIGNMENT_CENTER)
	mid.add_child(score_mode)
	mid.add_child(UI.label("152", 22, "regular", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER))
	mid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var us := VBoxContainer.new()
	us.add_theme_constant_override("separation", -8)
	us.add_child(UI.label("لنا", 26, "arabic", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER))
	score_us = UI.label("0", 52, "bold", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	us.add_child(score_us)
	us.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	s.add_child(them)
	s.add_child(mid)
	s.add_child(us)


func _clock_tick() -> void:
	var t := Time.get_time_dict_from_system()
	var h: int = t.hour % 12
	if h == 0:
		h = 12
	clock_label.text = "%d:%02d %s" % [h, t.minute, "PM" if t.hour >= 12 else "AM"]


func set_scores(us: int, them: int, mode_text: String) -> void:
	score_us.text = str(us)
	score_them.text = str(them)
	score_mode.text = mode_text


# ---------------------------------------------------------------- turn / bid panel
func _build_turn_panel() -> void:
	turn_panel = Glass.new()
	var c := turn_panel.setup(Vector2i(500, 720), 0.32, 52)
	_place(turn_panel, Vector3(0.64, 1.2, 0.02))
	var v := VBoxContainer.new()
	v.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	v.offset_left = 26
	v.offset_right = -26
	v.offset_top = 26
	v.add_theme_constant_override("separation", 14)
	c.add_child(v)
	var head := HBoxContainer.new()
	head.alignment = BoxContainer.ALIGNMENT_CENTER
	head.add_theme_constant_override("separation", 22)
	head.add_child(UI.icon("spade", 60, Color(0.7, 0.85, 1.0)))
	turn_title = UI.label("دورك", 62, "arabic", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	head.add_child(turn_title)
	v.add_child(head)
	var chip := PanelContainer.new()
	chip.add_theme_stylebox_override("panel", UI.flat_box(Color(1, 1, 1, 0.1), 26, Color(1, 1, 1, 0.18), 2))
	turn_sub = UI.label("", 36, "arabic", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	turn_sub.custom_minimum_size = Vector2(0, 76)
	chip.add_child(turn_sub)
	v.add_child(chip)
	turn_list = VBoxContainer.new()
	turn_list.add_theme_constant_override("separation", 12)
	v.add_child(turn_list)


func set_turn(title: String, sub: String) -> void:
	turn_title.text = title
	turn_sub.text = sub


func _bid_text(o: Dictionary) -> String:
	match o.bid:
		"sun":
			return "صن"
		"ashkal":
			return "أشكل"
		"pass":
			return "بس"
		"hokm":
			return "حكم %s" % R.SUIT_SYMBOLS[o.suit]
	return o.bid


func _bid_icon(o: Dictionary) -> String:
	return {"sun": "sun", "ashkal": "ashkal", "pass": "pass", "hokm": "hokm"}.get(o.bid, "spade")


func show_bid_options(options: Array, round2: bool) -> void:
	_clear(turn_list)
	for o in options:
		var text := _bid_text(o)
		if round2 and o.bid == "pass":
			text = "ولا"
		var b := UI.action_button(_bid_icon(o), text, 96, 448)
		if o.bid == "hokm" and (o.suit == 1 or o.suit == 2):
			(b.find_child("Text", true, false) as Label).add_theme_color_override("font_color", Color(1.0, 0.62, 0.62))
		b.pressed.connect(func(): bid_chosen.emit(o.bid, o.suit))
		turn_list.add_child(b)


func show_info_rows(rows: Array) -> void:
	_clear(turn_list)
	for r in rows:
		var p := PanelContainer.new()
		p.add_theme_stylebox_override("panel", UI.flat_box(Color(1, 1, 1, 0.05), 22))
		p.custom_minimum_size = Vector2(0, 86)
		var h := HBoxContainer.new()
		h.add_theme_constant_override("separation", 18)
		var well := UI.icon_well(r[0], 62, 0.12)
		well.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		h.add_child(well)
		var l := UI.label(r[1], 34, "arabic", r[2] if r.size() > 2 else UI.TEXT, HORIZONTAL_ALIGNMENT_RIGHT)
		l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(l)
		var m := MarginContainer.new()
		m.add_theme_constant_override("margin_left", 12)
		m.add_theme_constant_override("margin_right", 22)
		m.add_child(h)
		p.add_child(m)
		turn_list.add_child(p)


# ---------------------------------------------------------------- small pills
func _build_small_pills() -> void:
	count_pill = Glass.new()
	var h := _pill_container(count_pill, Vector2i(250, 110), 0.14, Vector3(-0.5, 0.86, 0.5), 55)
	h.add_child(UI.icon("people", 54))
	h.add_child(UI.label("4 / 4", 40, "medium"))

	chat_pill = Glass.new()
	var c := chat_pill.setup(Vector2i(260, 120), 0.16, 60, true)
	_place(chat_pill, Vector3(0.5, 0.88, 0.5))
	var row := HBoxContainer.new()
	row.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 14)
	var chat := UI.pill_button("chat", 96)
	chat.pressed.connect(func(): chat_pressed.emit())
	row.add_child(chat)
	var sep := ColorRect.new()
	sep.color = Color(1, 1, 1, 0.2)
	sep.custom_minimum_size = Vector2(2, 60)
	sep.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(sep)
	var mic := UI.pill_button("mic", 96)
	mic_icon = mic.find_child("Icon", true, false)
	mic.pressed.connect(func():
		_mic_on = not _mic_on
		mic_icon.modulate = UI.TEXT if _mic_on else UI.RED)
	row.add_child(mic)
	c.add_child(row)


# ---------------------------------------------------------------- name tags & bubbles
func _build_tag(seat: int, head_pos: Vector3) -> void:
	var tag := Glass.new()
	var c := tag.setup(Vector2i(340, 100), 0.26, 50, false)
	_place(tag, head_pos + Vector3(0, 0.27, 0))
	var h := HBoxContainer.new()
	h.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	h.offset_left = 28
	h.offset_right = -16
	h.add_theme_constant_override("separation", 10)
	var name_l := UI.label("", 38, "arabic")
	name_l.name = "Name"
	name_l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(name_l)
	var dot := Panel.new()
	dot.name = "Dot"
	dot.custom_minimum_size = Vector2(12, 12)
	dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	dot.add_theme_stylebox_override("panel", UI.flat_box(UI.GREEN, 6))
	h.add_child(dot)
	var badge := PanelContainer.new()
	badge.add_theme_stylebox_override("panel", UI.flat_box(Color(1, 1, 1, 0.14), 32, Color(1, 1, 1, 0.3), 2))
	badge.custom_minimum_size = Vector2(64, 64)
	badge.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var num := UI.label("0", 30, "medium", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	num.name = "Num"
	badge.add_child(num)
	h.add_child(badge)
	c.add_child(h)
	tags[seat] = tag

	var bubble := Glass.new()
	var bc := bubble.setup(Vector2i(420, 110), 0.3, 55, false, 0.7)
	_place(bubble, head_pos + Vector3(0, 0.45, 0))
	var bl := UI.label("", 46, "arabic", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	bl.name = "Text"
	bl.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	bc.add_child(bl)
	bubble.set_fade(0.0)
	bubbles[seat] = bubble


func add_self_bubble(pos: Vector3) -> void:
	var bubble := Glass.new()
	var bc := bubble.setup(Vector2i(460, 110), 0.26, 55, false, 0.7)
	_place(bubble, pos)
	var bl := UI.label("", 46, "arabic", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
	bl.name = "Text"
	bl.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	bc.add_child(bl)
	bubble.set_fade(0.0)
	bubbles[0] = bubble


func set_tag(seat: int, name_text: String, count: int, active: bool) -> void:
	var tag: Glass = tags[seat]
	(tag.content.find_child("Name", true, false) as Label).text = name_text
	(tag.content.find_child("Num", true, false) as Label).text = str(count)
	var dot: Panel = tag.content.find_child("Dot", true, false)
	dot.add_theme_stylebox_override("panel", UI.flat_box(UI.GOLD if active else UI.GREEN, 6))
	var sb := UI.glass_box(50, 0.8 if active else 0.74, 0.75 if active else 0.34)
	if active:
		sb.border_color = Color(UI.GOLD, 0.9)
	tag.glass.add_theme_stylebox_override("panel", sb)


func show_bubble(seat: int, text: String, duration: float = 1.8) -> void:
	var b: Glass = bubbles.get(seat)
	if b == null:
		return
	(b.content.find_child("Text", true, false) as Label).text = text
	b.show_animated(true, 0.2)
	var token: int = _bubble_tokens.get(seat, 0) + 1
	_bubble_tokens[seat] = token
	get_tree().create_timer(duration).timeout.connect(func():
		if _bubble_tokens.get(seat, 0) == token:
			b.show_animated(false, 0.3))


# ---------------------------------------------------------------- round summary
func _build_summary() -> void:
	summary = Glass.new()
	var c := summary.setup(Vector2i(760, 560), 0.44, 60, true, 0.55)
	_place(summary, Vector3(0, 1.28, -0.35))
	summary_box = VBoxContainer.new()
	summary_box.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	summary_box.offset_left = 40
	summary_box.offset_right = -40
	summary_box.offset_top = 30
	summary_box.offset_bottom = -30
	summary_box.add_theme_constant_override("separation", 12)
	c.add_child(summary_box)
	summary.set_fade(0.0)


## rows: [[label, us_value, them_value], ...]
func show_summary(title: String, subtitle: String, rows: Array, button_text: String) -> void:
	_clear(summary_box)
	summary_box.add_child(UI.label(title, 64, "arabic", UI.GOLD, HORIZONTAL_ALIGNMENT_CENTER))
	summary_box.add_child(UI.label(subtitle, 30, "arabic", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER))
	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 20)
	grid.add_theme_constant_override("v_separation", 4)
	for hdr in ["لهم", "", "لنا"]:
		var l := UI.label(hdr, 32, "arabic", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER)
		l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		grid.add_child(l)
	for r in rows:
		grid.add_child(UI.label(str(r[2]), 40, "bold", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER))
		grid.add_child(UI.label(r[0], 32, "arabic", UI.TEXT_DIM, HORIZONTAL_ALIGNMENT_CENTER))
		grid.add_child(UI.label(str(r[1]), 40, "bold", UI.TEXT, HORIZONTAL_ALIGNMENT_CENTER))
	summary_box.add_child(grid)
	var spacer := Control.new()
	spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	summary_box.add_child(spacer)
	var cc := CenterContainer.new()
	var b := UI.text_button(button_text, 96, 340)
	b.pressed.connect(func(): next_pressed.emit())
	cc.add_child(b)
	summary_box.add_child(cc)
	summary.show_animated(true, 0.3)


func hide_summary() -> void:
	summary.show_animated(false, 0.25)
