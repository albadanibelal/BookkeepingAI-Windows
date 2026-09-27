## Headless rules/AI soak test:  godot --headless --path . -s tests/sim.gd
extends SceneTree

const R = preload("res://scripts/rules.gd")
const AI = preload("res://scripts/ai.gd")
const Match = preload("res://scripts/match.gd")


func _init() -> void:
	var games := 300
	var rounds := 0
	var wins := [0, 0]
	var modes := {R.Mode.SUN: 0, R.Mode.HOKM: 0}
	var redeals := 0
	var kaboots := 0
	var failed := 0
	var projects_seen := 0
	for g in games:
		var m := Match.new(g)
		m.new_game()
		var guard := 0
		while m.phase != Match.Phase.GAME_OVER and guard < 200:
			guard += 1
			m.start_round()
			var bid_guard := 0
			while m.phase == Match.Phase.BIDDING:
				bid_guard += 1
				assert(bid_guard < 20)
				var p := m.turn
				var d := AI.choose_bid(m.hands[p], m.face_card, m.bid_round, R.suit_of(m.face_card), m.rng)
				var allowed := m.bid_options(p).any(func(o): return o.bid == d.bid and (d.bid != "hokm" or o.suit == d.suit))
				if not allowed:
					d = {"bid": "pass", "suit": -1}
				var res := m.apply_bid(p, d.bid, d.suit)
				if res.result == "redeal":
					redeals += 1
			if m.phase != Match.Phase.PLAYING:
				continue
			rounds += 1
			modes[m.mode] += 1
			for p in 4:
				assert(m.hands[p].size() == 8, "hand size %d" % m.hands[p].size())
				projects_seen += m.projects[p].size()
			var before := m.scores.duplicate()
			for t in 32:
				var p := m.turn
				var c := AI.choose_card(m.hands[p], m.trick, p, m.mode, m.trump, m.memory)
				var ev := m.play_card(p, c)
				if ev.round_done:
					var res: Dictionary = ev.result
					assert(res.raw[0] + res.raw[1] == (130 if m.mode == R.Mode.SUN else 162), "raw total %s" % [res.raw])
					if res.kaboot != -1:
						kaboots += 1
					if not res.made:
						failed += 1
			assert(m.phase == Match.Phase.ROUND_OVER or m.phase == Match.Phase.GAME_OVER)
			assert(m.scores[0] + m.scores[1] > before[0] + before[1])
		assert(m.phase == Match.Phase.GAME_OVER, "game %d did not finish" % g)
		wins[m.game_winner()] += 1
	# Rule spot checks
	var hand := [R.make_card(0, 0), R.make_card(0, 1), R.make_card(0, 2), R.make_card(1, 7), R.make_card(2, 7), R.make_card(3, 7), R.make_card(0, 6), R.make_card(1, 3)]
	var pr := R.find_projects(hand, R.Mode.SUN)
	assert(pr.size() == 1 and pr[0].type == "sira", "sira detection %s" % [pr])
	var aces := [R.make_card(0, 7), R.make_card(1, 7), R.make_card(2, 7), R.make_card(3, 7), R.make_card(0, 4), R.make_card(0, 5), R.make_card(0, 6), R.make_card(1, 0)]
	pr = R.find_projects(aces, R.Mode.SUN)
	assert(pr.size() == 2, "400 + sira expected %s" % [pr])
	var tr := [{"p": 0, "c": R.make_card(1, 7)}, {"p": 1, "c": R.make_card(0, 0)}]
	assert(R.trick_winner_index(tr, R.Mode.HOKM, 0) == 1, "trump should win")
	assert(R.trick_winner_index(tr, R.Mode.SUN, -1) == 0, "off-suit cannot win in sun")
	print("OK games=%d rounds=%d wins=%s sun=%d hokm=%d redeals=%d kaboot=%d failed_buys=%d projects=%d" % [games, rounds, wins, modes[R.Mode.SUN], modes[R.Mode.HOKM], redeals, kaboots, failed, projects_seen])
	quit(0)
