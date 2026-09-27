## Saudi Baloot rules: deck, card strength and points, legal moves, trick
## resolution, projects (مشاريع) and round scoring. Pure logic, no nodes.
extends RefCounted

enum Mode { NONE, SUN, HOKM }

const SUIT_CHARS := ["S", "H", "D", "C"]
const SUIT_SYMBOLS := ["♠", "♥", "♦", "♣"]
const SUIT_AR := ["سبيت", "هاص", "ديمن", "شريا"]
const RANK_NAMES := ["7", "8", "9", "10", "J", "Q", "K", "A"]

# Strength indexed by rank index (7,8,9,10,J,Q,K,A).
const PLAIN_STRENGTH := [0, 1, 2, 6, 3, 4, 5, 7]   # A 10 K Q J 9 8 7
const TRUMP_STRENGTH := [0, 1, 6, 4, 7, 2, 3, 5]   # J 9 A 10 K Q 8 7
const PLAIN_POINTS := [0, 0, 0, 10, 2, 3, 4, 11]
const TRUMP_POINTS := [0, 0, 14, 10, 20, 3, 4, 11]

const GAME_TARGET := 152
const LAST_TRICK_BONUS := 10


static func suit_of(c: int) -> int:
	return c / 8


static func rank_of(c: int) -> int:
	return c % 8


static func make_card(suit: int, rank: int) -> int:
	return suit * 8 + rank


static func card_id(c: int) -> String:
	return RANK_NAMES[rank_of(c)] + SUIT_CHARS[suit_of(c)]


static func card_label(c: int) -> String:
	return RANK_NAMES[rank_of(c)] + SUIT_SYMBOLS[suit_of(c)]


static func team_of(player: int) -> int:
	return player % 2


static func new_deck(rng: RandomNumberGenerator) -> Array:
	var deck := []
	for c in 32:
		deck.append(c)
	for i in range(deck.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var t = deck[i]
		deck[i] = deck[j]
		deck[j] = t
	return deck


static func is_trump(c: int, mode: int, trump: int) -> bool:
	return mode == Mode.HOKM and suit_of(c) == trump


static func strength(c: int, mode: int, trump: int) -> int:
	if is_trump(c, mode, trump):
		return TRUMP_STRENGTH[rank_of(c)]
	return PLAIN_STRENGTH[rank_of(c)]


static func points(c: int, mode: int, trump: int) -> int:
	if is_trump(c, mode, trump):
		return TRUMP_POINTS[rank_of(c)]
	return PLAIN_POINTS[rank_of(c)]


static func round_total(mode: int) -> int:
	return 26 if mode == Mode.SUN else 16


## Sort a hand for display: grouped by suit (alternating colours), strongest first.
static func sort_hand(hand: Array, mode: int, trump: int) -> Array:
	var order := [0, 1, 3, 2]  # ♠ ♥ ♣ ♦ keeps colours alternating
	var h := hand.duplicate()
	h.sort_custom(func(a, b):
		var sa := order.find(suit_of(a))
		var sb := order.find(suit_of(b))
		if mode == Mode.HOKM:
			sa += 10 if suit_of(a) == trump else 0
			sb += 10 if suit_of(b) == trump else 0
		if sa != sb:
			return sa < sb
		return strength(a, mode, trump) > strength(b, mode, trump))
	return h


## trick: Array of {"p": player, "c": card} in play order.
static func trick_winner_index(trick: Array, mode: int, trump: int) -> int:
	var best := 0
	var lead_suit := suit_of(trick[0].c)
	for i in range(1, trick.size()):
		var c: int = trick[i].c
		var b: int = trick[best].c
		if _beats(c, b, lead_suit, mode, trump):
			best = i
	return best


static func _beats(c: int, b: int, lead_suit: int, mode: int, trump: int) -> bool:
	var ct := is_trump(c, mode, trump)
	var bt := is_trump(b, mode, trump)
	if ct and not bt:
		return true
	if bt and not ct:
		return false
	if ct and bt:
		return strength(c, mode, trump) > strength(b, mode, trump)
	if suit_of(c) != lead_suit:
		return false
	if suit_of(b) != lead_suit:
		return true
	return strength(c, mode, trump) > strength(b, mode, trump)


static func trick_points(trick: Array, mode: int, trump: int) -> int:
	var total := 0
	for t in trick:
		total += points(t.c, mode, trump)
	return total


static func legal_moves(hand: Array, trick: Array, player: int, mode: int, trump: int) -> Array:
	if trick.is_empty():
		return hand.duplicate()
	var lead_suit := suit_of(trick[0].c)
	var follow := hand.filter(func(c): return suit_of(c) == lead_suit)
	var win_i := trick_winner_index(trick, mode, trump)
	var winning_card: int = trick[win_i].c
	var partner_winning := team_of(trick[win_i].p) == team_of(player)
	if not follow.is_empty():
		# Trump led: must go over the highest trump when possible.
		if mode == Mode.HOKM and lead_suit == trump:
			var higher := follow.filter(func(c): return strength(c, mode, trump) > strength(winning_card, mode, trump))
			if not higher.is_empty():
				return higher
		return follow
	if mode != Mode.HOKM or partner_winning:
		return hand.duplicate()
	var trumps := hand.filter(func(c): return suit_of(c) == trump)
	if trumps.is_empty():
		return hand.duplicate()
	if is_trump(winning_card, mode, trump):
		var over := trumps.filter(func(c): return strength(c, mode, trump) > strength(winning_card, mode, trump))
		return over if not over.is_empty() else hand.duplicate()
	return trumps


# ------------------------------------------------------------------ projects

## Returns an Array of projects: {"type": "sira"|"50"|"100"|"400", "raw": int, "top": int, "cards": Array}
static func find_projects(hand: Array, mode: int) -> Array:
	var projects := []
	var used := {}
	# Four of a kind: A (400 in sun / 100 in hokm), 10 K Q J (100).
	for r in [7, 3, 6, 5, 4]:
		var cards := hand.filter(func(c): return rank_of(c) == r)
		if cards.size() == 4:
			if r == 7:
				if mode == Mode.SUN:
					projects.append({"type": "400", "raw": 400, "top": 7, "cards": cards})
				else:
					projects.append({"type": "100", "raw": 100, "top": 7, "cards": cards})
			else:
				projects.append({"type": "100", "raw": 100, "top": r, "cards": cards})
			for c in cards:
				used[c] = true
	# Sequences in natural order 7 8 9 10 J Q K A, per suit.
	for s in 4:
		var ranks := []
		for c in hand:
			if suit_of(c) == s and not used.has(c):
				ranks.append(rank_of(c))
		ranks.sort()
		var run := []
		for i in ranks.size() + 1:
			if i < ranks.size() and (run.is_empty() or ranks[i] == run[-1] + 1):
				run.append(ranks[i])
				continue
			if run.size() >= 3:
				var cards := run.map(func(r): return make_card(s, r))
				var t := "sira" if run.size() == 3 else ("50" if run.size() == 4 else "100")
				var raw: int = {"sira": 20, "50": 50, "100": 100}[t]
				projects.append({"type": t, "raw": raw, "top": run[-1], "cards": cards})
			run = [ranks[i]] if i < ranks.size() else []
	return projects


static func project_name(t: String) -> String:
	return {"sira": "سرا", "50": "خمسين", "100": "مية", "400": "أربعمية"}.get(t, t)


static func project_game_points(raw: int, mode: int) -> int:
	# Sun: raw/10*2 (400 → 40). Hokm: raw/10.
	return raw * 2 / 10 if mode == Mode.SUN else raw / 10


static func has_baloot(hand: Array, mode: int, trump: int) -> bool:
	if mode != Mode.HOKM:
		return false
	return hand.has(make_card(trump, 6)) and hand.has(make_card(trump, 5))


## Decide which team's projects count. projects_by_player: Array[4] of project arrays.
## Returns the winning team (0/1) or -1 if nobody has a project.
static func project_winner_team(projects_by_player: Array, first_player: int) -> int:
	var best_team := -1
	var best_key := [-1, -1, -1]
	for k in 4:
		var p := (first_player + k) % 4
		for pr in projects_by_player[p]:
			var key := [pr.raw, pr.top, -k]
			if key[0] > best_key[0] or (key[0] == best_key[0] and (key[1] > best_key[1] or (key[1] == best_key[1] and key[2] > best_key[2]))):
				best_key = key
				best_team = team_of(p)
	return best_team


## Round scoring. Returns {"team": [a, b], "raw": [a, b], "projects": [a, b], "made": bool, "kaboot": int (team or -1)}
static func score_round(mode: int, buyer: int, raw: Array, tricks_won: Array, projects_by_player: Array,
		first_player: int, baloot_team: int) -> Dictionary:
	var buyer_team := team_of(buyer)
	var opp := 1 - buyer_team
	var proj := [0, 0]
	var pw := project_winner_team(projects_by_player, first_player)
	if pw != -1:
		for p in 4:
			if team_of(p) == pw:
				for pr in projects_by_player[p]:
					proj[pw] += project_game_points(pr.raw, mode)
	var total := round_total(mode)
	var result := [0, 0]
	var kaboot := -1
	if tricks_won[0] == 8 or tricks_won[1] == 8:
		kaboot = 0 if tricks_won[0] == 8 else 1
		result[kaboot] = (44 if mode == Mode.SUN else 25) + proj[kaboot]
	else:
		var b_pts: int
		if mode == Mode.SUN:
			b_pts = int(round(raw[buyer_team] * 2.0 / 10.0))
		else:
			b_pts = int(floor(raw[buyer_team] / 10.0 + 0.4))
		var o_pts := total - b_pts
		var b_total: int = b_pts + proj[buyer_team]
		var o_total: int = o_pts + proj[opp]
		if b_total < o_total:
			# خسرانة: the buyers lose everything to the other team.
			result[opp] = total + proj[0] + proj[1]
		else:
			result[buyer_team] = b_total
			result[opp] = o_total
	if baloot_team != -1:
		result[baloot_team] += 2
	var made: bool = result[buyer_team] > 0 and not (kaboot == opp)
	return {"team": result, "raw": raw, "projects": proj, "made": made, "kaboot": kaboot}
