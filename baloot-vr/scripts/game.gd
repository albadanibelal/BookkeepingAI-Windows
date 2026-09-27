## Presents a Baloot match around the majlis table: deals and animates cards,
## runs the bots, lets the local player bid and play with the laser pointer.
extends Node3D

const R = preload("res://scripts/rules.gd")
const AI = preload("res://scripts/ai.gd")
const Match = preload("res://scripts/match.gd")
const Card3D = preload("res://scripts/card3d.gd")
const Avatar = preload("res://scripts/avatar.gd")
const Hud = preload("res://scripts/ui/hud.gd")

signal human_bid(choice: Dictionary)
signal human_card(card: int)
signal state_changed

const TABLE_Y := 0.726
const HAND_SCALE := 1.3
const TABLE_SCALE := 1.55
const TAG_NAMES := {1: "Sara", 2: "Fahad", 3: "أبو خالد"}
const AR_NAMES := {0: "أنت", 1: "سارة", 2: "فهد", 3: "أبو خالد"}
const CHAT_LINES := ["يا هلا والله!", "حيّاكم الله", "ركّز يا شريك", "صكّة حلوة", "الله يعطيك العافية"]
const BOT_REPLIES := ["هلا بك", "أبشر", "على هونك علينا", "ما عليك منهم"]

var m: Match
var hud: Hud
var avatars := {}
var eye := Vector3(0, 1.2, 1.02)
var hand_root: Node3D
var hand_nodes: Array = []        # Card3D for seat 0, in display order
var table_cards: Array = []       # Card3D currently in the trick
var pile_nodes: Array = []        # won tricks (face down)
var deck_nodes: Array = []
var face_node: Card3D
var speed := 1.0
var auto_human := false           # let the AI play for the local player (demo / captures)
var running := false
var waiting_for_human := ""
var _chat_i := 0
var _generation := 0              # bumps on new game so stale coroutines stop


func setup(p_hud: Hud, p_avatars: Dictionary, p_eye: Vector3) -> void:
	hud = p_hud
	avatars = p_avatars
	eye = p_eye
	m = Match.new()
	hand_root = Node3D.new()
	add_child(hand_root)
	hand_root.position = Vector3(0, 0.93, 0.68)
	hand_root.look_at(eye, Vector3.UP, true)
	hud.bid_chosen.connect(_on_bid_chosen)
	hud.play_pressed.connect(func():
		if not running:
			start_new_game())
	hud.new_game_pressed.connect(start_new_game)
	hud.speed_toggled.connect(func(f): speed = 0.45 if f else 1.0)
	hud.chat_pressed.connect(_on_chat)
	_idle_ui()


func _idle_ui() -> void:
	hud.set_turn("بلوت", "حيّاك في المجلس")
	hud.show_bid_options([{"bid": "start", "suit": -1}], false)
	(hud.turn_list.get_child(0).find_child("Text", true, false) as Label).text = "ابدأ اللعب"
	for s in [1, 2, 3]:
		hud.set_tag(s, TAG_NAMES[s], 0, false)
		avatars[s].set_card_count(0)
	hud.set_scores(0, 0, "—")


func wait(t: float) -> void:
	if t * speed > 0.0:
		await get_tree().create_timer(t * speed).timeout


func _alive(gen: int) -> bool:
	return gen == _generation and is_inside_tree()


# ---------------------------------------------------------------- game loop
func start_new_game(seed_value: int = -1, first_dealer: int = -1) -> void:
	_generation += 1
	var gen := _generation
	_clear_table()
	m = Match.new(seed_value)
	m.new_game()
	if first_dealer >= 0:
		m.dealer = first_dealer
	running = true
	hud.hide_summary()
	hud.set_scores(0, 0, "—")
	while _alive(gen) and m.phase != Match.Phase.GAME_OVER:
		await _play_round(gen)
		if not _alive(gen):
			return
		if m.phase == Match.Phase.ROUND_OVER or m.phase == Match.Phase.GAME_OVER:
			await _show_round_summary(gen)
	running = false


func _play_round(gen: int) -> void:
	_clear_table()
	m.start_round()
	_look_all(Vector3(0, TABLE_Y, 0))
	await _animate_deal_first(gen)
	if not _alive(gen):
		return
	# ------------- bidding
	while _alive(gen) and m.phase == Match.Phase.BIDDING:
		var p := m.turn
		_set_active(p)
		var round2 := m.bid_round == 2
		var sub := "المزايدة • الجولة الثانية" if round2 else "المزايدة • الجولة الأولى"
		var choice: Dictionary
		if p == 0 and not auto_human:
			hud.set_turn("دورك", sub)
			hud.show_bid_options(m.bid_options(0), round2)
			waiting_for_human = "bid"
			choice = await human_bid
			waiting_for_human = ""
			if not _alive(gen):
				return
		else:
			hud.set_turn("دور " + AR_NAMES[p] if p != 0 else "دورك", sub)
			hud.show_info_rows([["spade", "المكشوفة: " + R.card_label(m.face_card)], ["people", "الموزّع: " + AR_NAMES[m.dealer]]])
			await wait(0.95)
			if not _alive(gen):
				return
			choice = AI.choose_bid(m.hands[p], m.face_card, m.bid_round, R.suit_of(m.face_card), m.rng)
			var allowed := m.bid_options(p).any(func(o): return o.bid == choice.bid and (choice.bid != "hokm" or o.suit == choice.suit))
			if not allowed:
				choice = {"bid": "pass", "suit": -1}
		hud.show_bubble(p, _bid_words(choice, round2))
		var res := m.apply_bid(p, choice.bid, choice.suit)
		if res.result == "redeal":
			await wait(0.8)
			hud.show_bubble(0, "ما أحد شرى — توزيع جديد", 2.0)
			await wait(1.4)
			await _play_round(gen)
			return
		if res.result == "round2":
			await wait(0.5)
	if not _alive(gen):
		return
	_announce_contract()
	await _animate_deal_rest(gen)
	if not _alive(gen):
		return
	# ------------- tricks
	var announced := {}
	while _alive(gen) and m.phase == Match.Phase.PLAYING:
		var p := m.turn
		_set_active(p)
		var card: int
		if p == 0 and not auto_human:
			hud.set_turn("دورك", "العب ورقة")
			_update_play_info()
			_enable_hand(true)
			waiting_for_human = "card"
			card = await human_card
			waiting_for_human = ""
			_enable_hand(false)
			if not _alive(gen):
				return
		else:
			hud.set_turn("دور " + AR_NAMES[p] if p != 0 else "دورك", "يلعب…")
			_update_play_info()
			await wait(0.8)
			if not _alive(gen):
				return
			card = AI.choose_card(m.hands[p], m.trick, p, m.mode, m.trump, m.memory)
		if m.tricks_played == 0 and not announced.has(p):
			announced[p] = true
			var names := []
			for pr in m.projects[p]:
				names.append(R.project_name(pr.type))
			if not names.is_empty():
				hud.show_bubble(p, "مشروع: " + " و ".join(names), 2.2)
		var ev := m.play_card(p, card)
		await _animate_play(p, card)
		if p == m.baloot_player and R.suit_of(card) == m.trump and (R.rank_of(card) == 6 or R.rank_of(card) == 5):
			var other := R.make_card(m.trump, 11 - R.rank_of(card))
			if not m.hands[p].has(other):
				hud.show_bubble(p, "بلوت!", 1.6)
		if ev.trick_done:
			_set_active(-1)
			_look_all(_trick_center())
			await wait(0.9)
			if not _alive(gen):
				return
			hud.show_bubble(ev.winner, "+%d" % ev.points, 1.2)
			await _collect_trick(ev.winner)
			_refresh_tags()
	_set_active(-1)


func _bid_words(c: Dictionary, round2: bool) -> String:
	match c.bid:
		"sun": return "صن"
		"ashkal": return "أشكل"
		"hokm": return "حكم " + R.SUIT_SYMBOLS[c.suit]
	return "ولا" if round2 else "بس"


func _mode_text() -> String:
	if m.mode == R.Mode.SUN:
		return "صن"
	if m.mode == R.Mode.HOKM:
		return "حكم " + R.SUIT_SYMBOLS[m.trump]
	return "—"


func _announce_contract() -> void:
	hud.set_scores(m.scores[0], m.scores[1], _mode_text())
	var who: String = AR_NAMES[m.buyer]
	hud.set_turn("اشترى " + who if m.buyer != 0 else "اشتريت", _mode_text())


func _update_play_info() -> void:
	var rows := []
	rows.append(["sun" if m.mode == R.Mode.SUN else "hokm", _mode_text(), Color(1.0, 0.7, 0.7) if m.trump == 1 or m.trump == 2 else Color.WHITE])
	rows.append(["profile", "المشتري: " + AR_NAMES[m.buyer]])
	rows.append(["trophy", "الأكلات: لنا %d • لهم %d" % [m.tricks_won[0], m.tricks_won[1]]])
	var mine := []
	for pr in m.projects[0]:
		mine.append(R.project_name(pr.type))
	if not mine.is_empty():
		rows.append(["ashkal", "مشاريعك: " + "، ".join(mine)])
	hud.show_info_rows(rows)


func _show_round_summary(gen: int) -> void:
	var res: Dictionary = m.last_result
	var buyer_team := R.team_of(m.buyer)
	var title := "انتهى الصكّ"
	if res.kaboot != -1:
		title = "كبوت!"
	elif not res.made:
		title = "خسرانة!"
	var sub := "%s • المشتري %s" % [_mode_text(), AR_NAMES[m.buyer]]
	var rows := [
		["الأكلات", res.raw[0], res.raw[1]],
		["المشاريع", res.projects[0], res.projects[1]],
		["نقاط الصكّ", res.team[0], res.team[1]],
		["المجموع", m.scores[0], m.scores[1]],
	]
	hud.set_scores(m.scores[0], m.scores[1], "—")
	var winner := m.game_winner()
	var btn := "التالي"
	if winner != -1:
		title = "مبروك! فزتوا" if winner == 0 else "هاردلك… فازوا"
		btn = "لعبة جديدة"
	_look_all(eye)
	hud.show_summary(title, sub, rows, btn)
	hud.set_turn("النتيجة", "لنا %d • لهم %d" % [m.scores[0], m.scores[1]])
	hud.show_info_rows([])
	if auto_human:
		await wait(2.5)
	else:
		await hud.next_pressed
	if not _alive(gen):
		return
	hud.hide_summary()
	if winner != -1:
		start_new_game.call_deferred()
	await wait(0.3)


# ---------------------------------------------------------------- human input
func _on_bid_chosen(bid: String, suit: int) -> void:
	if bid == "start":
		if not running:
			start_new_game()
		return
	if waiting_for_human == "bid":
		hud.show_info_rows([])
		human_bid.emit({"bid": bid, "suit": suit})


func _on_card_clicked(node) -> void:
	if waiting_for_human != "card":
		return
	if not m.legal_for(0).has(node.card):
		return
	human_card.emit(node.card)


func _on_card_hovered(node, on: bool) -> void:
	node.set_glow(on and node.playable)


func _enable_hand(on: bool) -> void:
	var legal := m.legal_for(0) if on else []
	for n in hand_nodes:
		n.set_interactive(on)
		n.set_playable(not on or legal.has(n.card))
		n.set_glow(false)


func _on_chat() -> void:
	hud.show_bubble(0, CHAT_LINES[_chat_i % CHAT_LINES.size()], 2.0)
	_chat_i += 1
	await get_tree().create_timer(1.2).timeout
	var s := randi_range(1, 3)
	hud.show_bubble(s, BOT_REPLIES[randi() % BOT_REPLIES.size()], 1.8)


# ---------------------------------------------------------------- presentation helpers
func _set_active(p: int) -> void:
	for s in [1, 2, 3]:
		hud.set_tag(s, TAG_NAMES[s], m.hands[s].size(), s == p)
	if p >= 0:
		_look_all(_seat_focus(p))


func _refresh_tags() -> void:
	for s in [1, 2, 3]:
		hud.set_tag(s, TAG_NAMES[s], m.hands[s].size(), false)
		avatars[s].set_card_count(m.hands[s].size())


func _look_all(target: Vector3) -> void:
	for s in avatars:
		avatars[s].look_target = target


func _seat_focus(p: int) -> Vector3:
	if p == 0:
		return eye
	return avatars[p].global_position + Vector3(0, 1.05, 0)


func _trick_center() -> Vector3:
	return Vector3(0, TABLE_Y, -0.06)


func _trick_slot(p: int) -> Transform3D:
	var offs := {0: Vector3(0, 0, 0.17), 1: Vector3(0.19, 0, -0.04), 2: Vector3(0, 0, -0.26), 3: Vector3(-0.19, 0, -0.04)}
	var yaw := {0: 0.0, 1: PI / 2, 2: PI, 3: -PI / 2}
	var b := (Basis(Vector3.UP, yaw[p] + randf_range(-0.12, 0.12)) * Basis(Vector3.RIGHT, -PI / 2)).scaled(Vector3.ONE * TABLE_SCALE)
	return Transform3D(b, _trick_center() + offs[p] * 1.2 + Vector3(0, 0.001 * table_cards.size(), 0))


func _pile_transform(team: int, index: int) -> Transform3D:
	var base := Vector3(0.5, TABLE_Y, 0.35) if team == 0 else Vector3(-0.52, TABLE_Y, -0.5)
	var b := Basis(Vector3.UP, 0.4 + index * 0.07) * Basis(Vector3.RIGHT, PI / 2)
	return Transform3D(b, base + Vector3(0, 0.0012 * index + 0.001, 0))


func _hand_slot(i: int, n: int) -> Transform3D:
	var t := i - (n - 1) / 2.0
	var tr := Transform3D.IDENTITY
	tr = tr.rotated(Vector3.FORWARD, t * 0.07)
	tr.origin = Vector3(t * 0.05, -absf(t) * absf(t) * 0.0026, t * 0.0015)
	tr.basis = tr.basis.scaled(Vector3.ONE * HAND_SCALE)
	return hand_root.global_transform * tr


func _layout_hand(animated: bool = true) -> void:
	var n := hand_nodes.size()
	for i in n:
		var node: Card3D = hand_nodes[i]
		var target := _hand_slot(i, n)
		node.base_transform = target
		if animated:
			var tw := node.create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
			tw.tween_property(node, "global_transform", target, 0.25 * speed)
		else:
			node.global_transform = target


func _new_card(c: int) -> Card3D:
	var node := Card3D.new(c)
	add_child(node)
	return node


func _clear_table() -> void:
	for arr in [hand_nodes, table_cards, pile_nodes, deck_nodes]:
		for n in arr:
			if is_instance_valid(n):
				n.queue_free()
		arr.clear()
	if face_node and is_instance_valid(face_node):
		face_node.queue_free()
	face_node = null
	for s in [1, 2, 3]:
		avatars[s].set_card_count(0)


func _deck_transform(i: int) -> Transform3D:
	var b := (Basis(Vector3.UP, 0.3) * Basis(Vector3.RIGHT, PI / 2)).scaled(Vector3.ONE * 1.3)
	return Transform3D(b, Vector3(-0.12, TABLE_Y + 0.0009 * i, -0.08))


func _build_deck(n: int) -> void:
	for i in n:
		var d := _new_card(-1)
		d.global_transform = _deck_transform(i)
		deck_nodes.append(d)


func _fly(node: Node3D, target: Transform3D, duration: float, arc: float = 0.08) -> void:
	var start := node.global_transform
	var tw := node.create_tween()
	tw.tween_method(func(t: float):
		var e := 1.0 - pow(1.0 - t, 3.0)
		var tr := start.interpolate_with(target, e)
		tr.origin.y += sin(t * PI) * arc
		node.global_transform = tr, 0.0, 1.0, maxf(duration * speed, 0.01))
	await tw.finished


func _animate_deal_first(gen: int) -> void:
	_build_deck(32)
	await wait(0.3)
	# 5 cards each, dealt from the top of the deck
	for k in 5:
		for q in 4:
			if not _alive(gen):
				return
			var p := (m.dealer + 1 + q) % 4
			await _deal_one(p, m.hands[p][k] if p == 0 else -1)
	# face-up card
	var top: Card3D = deck_nodes.pop_back()
	top.set_card(m.face_card)
	face_node = top
	var b := (Basis(Vector3.UP, -0.2) * Basis(Vector3.RIGHT, -PI / 2)).scaled(Vector3.ONE * TABLE_SCALE)
	await _fly(top, Transform3D(b, Vector3(0.14, TABLE_Y + 0.002, -0.02)), 0.4, 0.12)
	hud.show_bubble(0, "المكشوفة " + R.card_label(m.face_card), 1.6)


func _deal_one(p: int, card: int) -> void:
	var node: Card3D = deck_nodes.pop_back()
	if p == 0:
		node.set_card(card)
		hand_nodes.append(node)
		node.hovered.connect(_on_card_hovered)
		node.clicked.connect(_on_card_clicked)
		var n := hand_nodes.size()
		_fly(node, _hand_slot(n - 1, max(n, 5)), 0.22, 0.1)
		await wait(0.06)
	else:
		var target := Transform3D(Basis.IDENTITY, avatars[p].hand_point())
		await _fly(node, target, 0.16, 0.06)
		node.queue_free()
		avatars[p].set_card_count(avatars[p].hand_cards.size() + 1)


func _animate_deal_rest(gen: int) -> void:
	# face-up card goes to the taker
	if face_node:
		if m.taker == 0:
			hand_nodes.append(face_node)
			face_node.hovered.connect(_on_card_hovered)
			face_node.clicked.connect(_on_card_clicked)
			await _fly(face_node, _hand_slot(hand_nodes.size() - 1, hand_nodes.size()), 0.3, 0.1)
		else:
			await _fly(face_node, Transform3D(Basis.IDENTITY, avatars[m.taker].hand_point()), 0.3, 0.1)
			face_node.queue_free()
			avatars[m.taker].set_card_count(avatars[m.taker].hand_cards.size() + 1)
		face_node = null
	for q in 4:
		var p := (m.dealer + 1 + q) % 4
		var n := 2 if p == m.taker else 3
		for k in n:
			if not _alive(gen):
				return
			await _deal_one(p, -2 if p == 0 else -1)
	# Reveal the local player's final, sorted hand.
	var sorted: Array = m.hands[0]
	for i in hand_nodes.size():
		hand_nodes[i].set_card(sorted[i])
	_layout_hand(true)
	for n in deck_nodes:
		n.queue_free()
	deck_nodes.clear()
	_refresh_tags()
	await wait(0.4)


func _animate_play(p: int, card: int) -> void:
	var node: Card3D
	if p == 0:
		for n in hand_nodes:
			if n.card == card:
				node = n
		hand_nodes.erase(node)
		node.set_interactive(false)
		node.set_glow(false)
		_layout_hand(true)
	else:
		node = _new_card(card)
		node.global_transform = Transform3D(Basis(Vector3.UP, avatars[p].global_rotation.y), avatars[p].hand_point())
		avatars[p].set_card_count(m.hands[p].size())
		hud.set_tag(p, TAG_NAMES[p], m.hands[p].size(), true)
	table_cards.append(node)
	_look_all(_trick_center())
	await _fly(node, _trick_slot(p), 0.35, 0.1)


func _collect_trick(winner: int) -> void:
	var team := R.team_of(winner)
	var count := 0
	for n in pile_nodes:
		if n.get_meta("team") == team:
			count += 1
	for n in table_cards:
		var node: Card3D = n
		node.set_meta("team", team)
		var target := _pile_transform(team, count)
		_fly(node, target, 0.4, 0.05)
		pile_nodes.append(node)
	table_cards.clear()
	await wait(0.45)
	# once face-down on the pile only the back should show
	for n in pile_nodes:
		if is_instance_valid(n):
			n.set_card(-1)
