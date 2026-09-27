## Heuristic Baloot bots: bidding by hand strength, and card play that
## remembers what has been played, feeds points to a winning partner and
## wins tricks as cheaply as possible.
extends RefCounted

const R = preload("res://scripts/rules.gd")


## Returns {"bid": "sun"|"hokm"|"pass", "suit": int}
static func choose_bid(hand: Array, face_card: int, bid_round: int, forbidden_suit: int, rng: RandomNumberGenerator) -> Dictionary:
	var with_face := hand.duplicate()
	with_face.append(face_card)
	var sun := _sun_strength(with_face)
	var best_suit := -1
	var best_hokm := 0.0
	if bid_round == 1:
		best_suit = R.suit_of(face_card)
		best_hokm = _hokm_strength(with_face, best_suit)
	else:
		for s in 4:
			if s == forbidden_suit:
				continue
			var v := _hokm_strength(with_face, s)
			if v > best_hokm:
				best_hokm = v
				best_suit = s
	var jitter := rng.randf_range(-0.6, 0.6)
	if sun + jitter >= 7.0 and sun >= best_hokm - 1.0:
		return {"bid": "sun", "suit": -1}
	var need := 5.5 if bid_round == 1 else 5.0
	if best_suit != -1 and best_hokm + jitter >= need:
		return {"bid": "hokm", "suit": best_suit}
	return {"bid": "pass", "suit": -1}


static func _sun_strength(cards: Array) -> float:
	var v := 0.0
	for c in cards:
		var r := R.rank_of(c)
		match r:
			7: v += 3.0
			3:
				v += 1.8 if cards.has(R.make_card(R.suit_of(c), 7)) else 0.8
			6: v += 0.6
	return v


static func _hokm_strength(cards: Array, s: int) -> float:
	var v := 0.0
	var count := 0
	for c in cards:
		if R.suit_of(c) == s:
			count += 1
			match R.rank_of(c):
				4: v += 3.5
				2: v += 2.5
				7: v += 1.8
				3: v += 1.2
				_: v += 0.7
		elif R.rank_of(c) == 7:
			v += 1.3
	if count <= 1:
		v -= 2.0
	return v


## memory: Dictionary card -> true for every card already played this round.
static func choose_card(hand: Array, trick: Array, player: int, mode: int, trump: int, memory: Dictionary) -> int:
	var legal := R.legal_moves(hand, trick, player, mode, trump)
	if legal.size() == 1:
		return legal[0]
	if trick.is_empty():
		return _lead(legal, hand, mode, trump, memory)
	var lead_suit := R.suit_of(trick[0].c)
	var win_i := R.trick_winner_index(trick, mode, trump)
	var winner: int = trick[win_i].p
	var partner_winning := R.team_of(winner) == R.team_of(player)
	var last_to_play := trick.size() == 3
	if partner_winning and (last_to_play or _is_master(trick[win_i].c, mode, trump, memory, hand, trick)):
		return _highest_points(legal, mode, trump, true)
	var winners := legal.filter(func(c): return _would_win(c, trick, player, mode, trump))
	if not winners.is_empty():
		if last_to_play:
			return _cheapest(winners, mode, trump)
		var masters := winners.filter(func(c): return _is_master(c, mode, trump, memory, hand, trick))
		if not masters.is_empty():
			return _cheapest(masters, mode, trump)
		if R.trick_points(trick, mode, trump) >= 10:
			return _cheapest(winners, mode, trump)
	# Can't (or shouldn't) win: throw the least valuable card.
	return _lowest_value(legal, mode, trump, lead_suit)


static func _would_win(c: int, trick: Array, player: int, mode: int, trump: int) -> bool:
	var t := trick.duplicate()
	t.append({"p": player, "c": c})
	return R.trick_winner_index(t, mode, trump) == t.size() - 1


## A card is "master" when no unseen card of its suit is stronger.
static func _is_master(c: int, mode: int, trump: int, memory: Dictionary, hand: Array, trick: Array) -> bool:
	var s := R.suit_of(c)
	var st := R.strength(c, mode, trump)
	for r in 8:
		var other := R.make_card(s, r)
		if other == c or memory.has(other) or hand.has(other):
			continue
		var in_trick := false
		for t in trick:
			if t.c == other:
				in_trick = true
		if in_trick:
			continue
		if R.strength(other, mode, trump) > st:
			return false
	return true


static func _lead(legal: Array, hand: Array, mode: int, trump: int, memory: Dictionary) -> int:
	# Pull trumps with the master trump when we hold several.
	if mode == R.Mode.HOKM:
		var trumps := legal.filter(func(c): return R.suit_of(c) == trump)
		if trumps.size() >= 3:
			for c in trumps:
				if _is_master(c, mode, trump, memory, hand, []):
					return c
	var masters := legal.filter(func(c): return not R.is_trump(c, mode, trump) and _is_master(c, mode, trump, memory, hand, []))
	if not masters.is_empty():
		masters.sort_custom(func(a, b): return R.points(a, mode, trump) > R.points(b, mode, trump))
		return masters[0]
	# Otherwise lead a small card from the longest non-trump suit.
	var by_suit := {}
	for c in legal:
		if R.is_trump(c, mode, trump):
			continue
		by_suit[R.suit_of(c)] = by_suit.get(R.suit_of(c), 0) + 1
	var best_suit := -1
	var best_n := 0
	for s in by_suit:
		if by_suit[s] > best_n:
			best_n = by_suit[s]
			best_suit = s
	var pool := legal.filter(func(c): return R.suit_of(c) == best_suit) if best_suit != -1 else legal
	return _cheapest(pool, mode, trump)


static func _cheapest(cards: Array, mode: int, trump: int) -> int:
	var best: int = cards[0]
	for c in cards:
		var a := R.strength(c, mode, trump) + (20 if R.is_trump(c, mode, trump) else 0)
		var b := R.strength(best, mode, trump) + (20 if R.is_trump(best, mode, trump) else 0)
		if a < b:
			best = c
	return best


static func _highest_points(cards: Array, mode: int, trump: int, avoid_trump: bool) -> int:
	var pool := cards
	if avoid_trump:
		var non := cards.filter(func(c): return not R.is_trump(c, mode, trump))
		if not non.is_empty():
			pool = non
	var best: int = pool[0]
	for c in pool:
		if R.points(c, mode, trump) > R.points(best, mode, trump):
			best = c
	return best


static func _lowest_value(cards: Array, mode: int, trump: int, _lead_suit: int) -> int:
	var best: int = cards[0]
	var best_v := 1_000_000
	for c in cards:
		var v := R.points(c, mode, trump) * 10 + R.strength(c, mode, trump) + (100 if R.is_trump(c, mode, trump) else 0)
		if v < best_v:
			best_v = v
			best = c
	return best
