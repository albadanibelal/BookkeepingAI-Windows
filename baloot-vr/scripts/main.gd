## Entry point: assembles the majlis, the companions, the player rig, the
## glass HUD and the Baloot table controller.
##
## Desktop capture helpers (used to make store screenshots / trailers):
##   godot --path . -- --scenario=bid|play|menu --shot=out.png
##   godot --path . --write-movie out.avi -- --scenario=demo --quit-after-sec=40
extends Node3D

const Env = preload("res://scripts/environment.gd")
const Avatar = preload("res://scripts/avatar.gd")
const Rig = preload("res://scripts/player_rig.gd")
const Hud = preload("res://scripts/ui/hud.gd")
const Game = preload("res://scripts/game.gd")
const Match = preload("res://scripts/match.gd")

const EYE := Vector3(0, 1.2, 1.02)
const SEATS := {
	1: {"pos": Vector3(1.3, 0, -0.34), "style": Avatar.Style.HIJAB, "skin": Color(0.86, 0.68, 0.56)},
	2: {"pos": Vector3(0, 0, -1.42), "style": Avatar.Style.GHUTRA_WHITE, "skin": Color(0.8, 0.6, 0.46)},
	3: {"pos": Vector3(-1.3, 0, -0.34), "style": Avatar.Style.SHEMAGH_RED, "skin": Color(0.76, 0.55, 0.41)},
}

var env: Env
var rig: Rig
var hud: Hud
var game: Game
var avatars := {}
var args := {}


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		var kv := a.trim_prefix("--").split("=", true, 1)
		args[kv[0]] = kv[1] if kv.size() > 1 else "1"
	rig = Rig.new()
	add_child(rig)
	var xr := rig.setup(EYE)
	env = Env.new()
	add_child(env)
	env.build(xr)
	var heads := {}
	for s in SEATS:
		var a := Avatar.new()
		add_child(a)
		a.position = SEATS[s].pos
		a.look_at(Vector3(0, 0, -0.1), Vector3.UP, true)
		a.build(SEATS[s].style, SEATS[s].skin, s * 1.3)
		avatars[s] = a
		heads[s] = a.global_position + Vector3(0, 1.14, 0) + (Vector3(0, 0, -0.1) - a.global_position).normalized() * 0.05
	hud = Hud.new()
	add_child(hud)
	hud.build(EYE, heads)
	hud.add_self_bubble(Vector3(0, 1.02, 0.12))
	game = Game.new()
	add_child(game)
	game.setup(hud, avatars, EYE)
	hud.recenter_pressed.connect(rig.recenter)
	if args.has("cam"):
		# --cam=x,y,z,tx,ty,tz  (desktop captures only)
		var v: PackedFloat64Array = str(args["cam"]).split_floats(",")
		rig.camera.global_position = Vector3(v[0], v[1], v[2])
		rig.camera.look_at(Vector3(v[3], v[4], v[5]))
	if args.has("scenario"):
		_run_scenario(args["scenario"])


func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_F12:
		_save_shot("user://shot_%d.png" % Time.get_ticks_msec())


func _save_shot(path: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(path)
	print("saved ", path)


func _frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame


func _run_scenario(name: String) -> void:
	var shot: String = args.get("shot", "")
	match name:
		"menu":
			await _frames(30)
		"bid":
			game.speed = 0.15
			game.start_new_game(11, 3)
			while not (game.waiting_for_human == "bid"):
				await get_tree().process_frame
			game.speed = 1.0
			await get_tree().create_timer(1.2).timeout
			rig.aim_desktop_at(hud.turn_panel.global_position + Vector3(0, -0.05, 0))
			await _frames(10)
		"play":
			game.speed = 0.05
			game.auto_human = true
			game.start_new_game(int(args.get("seed", "5")), 3)
			# fast-forward until the local player is about to play into a trick
			while not (game.m.phase == Match.Phase.PLAYING and game.m.tricks_played >= int(args.get("trick", "2")) and game.m.turn == 0 and game.m.trick.size() >= 2):
				await get_tree().process_frame
			game.auto_human = false
			game.speed = 1.0
			while game.waiting_for_human != "card":
				await get_tree().process_frame
			await get_tree().create_timer(1.0).timeout
			var legal: Array = game.m.legal_for(0)
			for n in game.hand_nodes:
				if legal.has(n.card):
					rig.aim_desktop_at(n.global_position + n.global_basis.y * 0.02)
			await get_tree().create_timer(0.6).timeout
		"demo":
			game.auto_human = true
			game.speed = float(args.get("speed", "0.8"))
			game.start_new_game(int(args.get("seed", "7")), 3)
			return
	if args.has("dumpui"):
		await RenderingServer.frame_post_draw
		hud.menu.viewport.get_texture().get_image().save_png(str(args["dumpui"]))
		print("menu vp size ", hud.menu.viewport.size, " content ", hud.menu.content.size, " quad ", (hud.menu.quad.mesh as QuadMesh).size)
	if shot != "":
		await _save_shot(shot)
		get_tree().quit()
