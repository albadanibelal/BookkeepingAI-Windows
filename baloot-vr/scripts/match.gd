## Pure Baloot match state machine (no nodes, no timing) so it can be driven by
## the 3D table or by the headless simulation in tests/sim.gd.
##
## Seats: 0 = local player (south), 1 = right, 2 = partner (across), 3 = left.
## Play and bidding go counter-clockwise: 0 → 1 → 2 → 3.
extends RefCounted

const R = preload("res://scripts/rules.gd")

enum Phase { IDLE, BIDDING, PLAYING, ROUND_OVER, GAME_OVER }

var rng := RandomNumberGenerator.new()
var phase: int = Phase.IDLE
var scores := [0, 0]
var dealer := 3
var round_number := 0

var deck: Array = []
var hands: Array = [[], [], [], []]
var face_card := -1

var bid_round := 1
var bid_turns := 0            # how many players have acted in the current bid round
var pending_hokm := -1        # player holding a round-1 hokm that others may still upgrade
var turn := 0
var bids_log: Array = []      # [{p, bid, suit}]

var mode: int = R.Mode.NONE
var trump := -1
var buyer := -1               # player who won the bid
var taker := -1               # player who receives the face card (partner on أشكل)

var trick: Array = []         # [{p, c}]
var last_trick: Array = []
var tricks_played := 0
var tricks_won := [0, 0]
var raw := [0, 0]
var memory := {}
var projects: Array = [[], [], [], []]
var baloot_team := -1
var baloot_player := -1
var last_result := {}


func _init(seed_value: int = -1) -> void:
	if seed_value >= 0:
		rng.seed = seed_value
	else:
		rng.randomize()


func new_game() -> void:
	scores = [0, 0]
	round_number = 0
	dealer = rng.randi_range(0, 3)
	phase = Phase.IDLE


func start_round() -> void:
	round_number += 1
	deck = R.new_deck(rng)
	hands = [[], [], [], []]
	for k in 5:
		for p in 4:
			hands[(dealer + 1 + p) % 4].append(deck.pop_back())
	face_card = deck.pop_back()
	bid_round = 1
	bid_turns = 0
	pending_hokm = -1
	bids_log = []
	mode = R.Mode.NONE
	trump = -1
	buyer = -1
	taker = -1
	trick = []
	last_trick = []
	tricks_played = 0
	tricks_won = [0, 0]
	raw = [0, 0]
	memory = {}
	projects = [[], [], [], []]
	baloot_team = -1
	baloot_player = -1
	turn = (dealer + 1) % 4
	phase = Phase.BIDDING


## Bid options for the player whose turn it is.
func bid_options(player: int) -> Array:
	var opts := []
	if bid_round == 1:
		if pending_hokm == -1:
			opts.append({"bid": "hokm", "suit": R.suit_of(face_card)})
		opts.append({"bid": "sun", "suit": -1})
		# أشكل is reserved for the last two bidders (the dealer and the player before).
		if pending_hokm == -1 and bid_turns >= 2:
			opts.append({"bid": "ashkal", "suit": -1})
		opts.append({"bid": "pass", "suit": -1})
	else:
		for s in 4:
			if s != R.suit_of(face_card):
				opts.append({"bid": "hokm", "suit": s})
		opts.append({"bid": "sun", "suit": -1})
		opts.append({"bid": "pass", "suit": -1})
	return opts


## Returns {"result": "next"|"round2"|"redeal"|"final", ...}
func apply_bid(player: int, bid: String, suit: int = -1) -> Dictionary:
	assert(phase == Phase.BIDDING and player == turn)
	bids_log.append({"p": player, "bid": bid, "suit": suit, "round": bid_round})
	bid_turns += 1
	match bid:
		"sun":
			_finalize_bid(player, R.Mode.SUN, -1, player)
			return {"result": "final"}
		"ashkal":
			_finalize_bid(player, R.Mode.SUN, -1, (player + 2) % 4)
			return {"result": "final"}
		"hokm":
			if bid_round == 2:
				_finalize_bid(player, R.Mode.HOKM, suit, player)
				return {"result": "final"}
			pending_hokm = player
	if bid_turns >= 4:
		if pending_hokm != -1:
			_finalize_bid(pending_hokm, R.Mode.HOKM, R.suit_of(face_card), pending_hokm)
			return {"result": "final"}
		if bid_round == 1:
			bid_round = 2
			bid_turns = 0
			turn = (dealer + 1) % 4
			return {"result": "round2"}
		# Everybody passed twice: reshuffle, next dealer.
		dealer = (dealer + 1) % 4
		round_number -= 1
		phase = Phase.IDLE
		return {"result": "redeal"}
	turn = (turn + 1) % 4
	# With a pending hokm, only players who have not spoken yet may upgrade to sun.
	return {"result": "next"}


func _finalize_bid(p: int, m: int, s: int, take: int) -> void:
	buyer = p
	mode = m
	trump = s
	taker = take
	hands[taker].append(face_card)
	for k in 2:
		hands[taker].append(deck.pop_back())
	for q in 4:
		if q == taker:
			continue
		for k in 3:
			hands[q].append(deck.pop_back())
	for q in 4:
		hands[q] = R.sort_hand(hands[q], mode, trump)
		projects[q] = R.find_projects(hands[q], mode)
		if R.has_baloot(hands[q], mode, trump):
			baloot_team = R.team_of(q)
			baloot_player = q
	turn = (dealer + 1) % 4
	phase = Phase.PLAYING


func legal_for(player: int) -> Array:
	return R.legal_moves(hands[player], trick, player, mode, trump)


## Returns {"trick_done": bool, "winner": int, "points": int, "round_done": bool, "result": Dictionary}
func play_card(player: int, card: int) -> Dictionary:
	assert(phase == Phase.PLAYING and player == turn)
	assert(legal_for(player).has(card), "illegal card %s" % R.card_id(card))
	hands[player].erase(card)
	trick.append({"p": player, "c": card})
	memory[card] = true
	var ev := {"trick_done": false, "winner": -1, "points": 0, "round_done": false, "result": {}}
	if trick.size() < 4:
		turn = (turn + 1) % 4
		return ev
	var wi := R.trick_winner_index(trick, mode, trump)
	var winner: int = trick[wi].p
	var pts := R.trick_points(trick, mode, trump)
	tricks_played += 1
	if tricks_played == 8:
		pts += R.LAST_TRICK_BONUS
	raw[R.team_of(winner)] += pts
	tricks_won[R.team_of(winner)] += 1
	last_trick = trick
	trick = []
	turn = winner
	ev.trick_done = true
	ev.winner = winner
	ev.points = pts
	if tricks_played == 8:
		last_result = R.score_round(mode, buyer, raw, tricks_won, projects, (dealer + 1) % 4, baloot_team)
		scores[0] += last_result.team[0]
		scores[1] += last_result.team[1]
		dealer = (dealer + 1) % 4
		ev.round_done = true
		ev.result = last_result
		phase = Phase.GAME_OVER if game_winner() != -1 else Phase.ROUND_OVER
	return ev


func game_winner() -> int:
	if scores[0] < R.GAME_TARGET and scores[1] < R.GAME_TARGET:
		return -1
	if scores[0] == scores[1]:
		return -1
	return 0 if scores[0] > scores[1] else 1
