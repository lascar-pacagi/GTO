import random, json, time
from collections import defaultdict

# --- Game setup ---
DECK = [0,0,1,1,2,2]  # 0=J,1=Q,2=K
ACTION_NO_BET = ['check', 'bet']
ACTION_BET = ['fold', 'call']

def hand_rank(private, public):
    if private == public:
        return (1, private)  # pair
    return (0, private)     # high card

def showdown_winner(cards, public):
    r1, r2 = hand_rank(cards[0], public), hand_rank(cards[1], public)
    if r1[0] != r2[0]:
        return 0 if r1[0] > r2[0] else 1
    if r1[1] != r2[1]:
        return 0 if r1[1] > r2[1] else 1
    return None  # tie

# --- CFR Solver ---
class LeducCFR:
    def __init__(self, raise_cap=1):
        self.regret_sum = defaultdict(list)
        self.strategy_sum = defaultdict(list)
        self.raise_cap = raise_cap

    def info_set_key(self, player, private, public, history, round_no, bet_pending, raises_in_round):
        pub = -1 if public is None else public
        bp = 1 if bet_pending else 0
        return f"P{player}_card{private}_pub{pub}_r{round_no}_hist{history}_bp{bp}_ra{raises_in_round}"

    def get_action_list(self, bet_pending):
        return ACTION_BET if bet_pending else ACTION_NO_BET

    def get_strategy(self, info_set, n_actions):
        if info_set not in self.regret_sum:
            self.regret_sum[info_set] = [0.0] * n_actions
            self.strategy_sum[info_set] = [0.0] * n_actions
        regrets = self.regret_sum[info_set]
        pos = [r if r > 0 else 0.0 for r in regrets]
        total = sum(pos)
        strat = [p/total for p in pos] if total > 0 else [1.0/n_actions] * n_actions
        for i in range(n_actions):
            self.strategy_sum[info_set][i] += strat[i]
        return strat

    def get_average_strategy(self, info_set):
        s = self.strategy_sum[info_set]
        total = sum(s)
        if total > 0:
            return [x/total for x in s]
        return [1.0/len(s)] * len(s) if s else None

    def terminal_check(self, history, contributions, round_no, public_card, cards):
        if history.endswith('f'):  # fold
            pot = sum(contributions)
            actions_only = history.replace('|','')
            last_actor = (len(actions_only)-1) % 2
            winner = 1 - last_actor
            return True, pot if winner == 0 else -pot
        if '|' in history:
            post = history.split('|')[-1]
            if len(post)>0 and post[-1] == 'c':  # round finished -> showdown
                pot = sum(contributions)
                winner = showdown_winner(cards, public_card)
                return True, 0.0 if winner is None else (pot if winner==0 else -pot)
        else:
            if len(history)>0 and history[-1] == 'c':
                return False, None
        return False, None

    def cfr(self, cards, public_card, history, round_no, contributions, p0, p1, raises_in_round):
        terminal, payoff = self.terminal_check(history, contributions, round_no, public_card, cards)
        if terminal: return payoff

        plays = len(history.replace('|',''))
        player = plays % 2
        bet_pending = history.replace('|','').endswith('b')

        private = cards[player]
        info_key = self.info_set_key(player, private, public_card, history, round_no, bet_pending, raises_in_round)
        actions = self.get_action_list(bet_pending)
        strategy = self.get_strategy(info_key, len(actions))

        util, node_util = [0.0]*len(actions), 0.0
        for a_idx, act in enumerate(actions):
            ch = 'c' if act in ['check','call'] else ('b' if act=='bet' else 'f')
            next_history = history + ch
            next_contribs, next_round, next_raises = contributions.copy(), round_no, raises_in_round
            if act == 'bet':
                next_contribs[player] += 1; next_raises += 1
            elif act == 'call':
                next_contribs[player] += 1

            # Pre-round ends → deal public card
            if round_no == 1:
                bet_pending_next = next_history.replace('|','').endswith('b')
                pre_round_ended = (next_history.split('|')[-1].endswith('c') and not bet_pending_next)
                if pre_round_ended:
                    deck = DECK.copy()
                    deck.remove(cards[0]); deck.remove(cards[1])
                    util_sum = 0.0
                    for pub in deck:
                        util_sum += self.cfr(cards, pub, next_history+'|', 2, next_contribs, p0, p1, 0)
                    node_util_action = util_sum / len(deck)
                else:
                    if player == 0:
                        node_util_action = -self.cfr(cards, public_card, next_history, round_no, next_contribs, p0*strategy[a_idx], p1, next_raises)
                    else:
                        node_util_action = -self.cfr(cards, public_card, next_history, round_no, next_contribs, p0, p1*strategy[a_idx], next_raises)
            else:
                if player == 0:
                    node_util_action = -self.cfr(cards, public_card, next_history, round_no, next_contribs, p0*strategy[a_idx], p1, next_raises)
                else:
                    node_util_action = -self.cfr(cards, public_card, next_history, round_no, next_contribs, p0, p1*strategy[a_idx], next_raises)

            util[a_idx] = node_util_action
            node_util += strategy[a_idx] * node_util_action

        # Regret update
        for a_idx in range(len(actions)):
            regret = util[a_idx] - node_util
            if player == 0:
                self.regret_sum[info_key][a_idx] += p1 * regret
            else:
                self.regret_sum[info_key][a_idx] += p0 * regret

        return node_util

    def train(self, iterations=1_000_000):
        util = 0.0
        for _ in range(iterations):
            deck = DECK.copy()
            random.shuffle(deck)
            cards = [deck.pop(), deck.pop()]
            contributions = [1,1]
            util += self.cfr(cards, None, "", 1, contributions, 1.0, 1.0, 0)
        return util / iterations

# --- Run training ---
if __name__ == "__main__":
    random.seed(42)
    solver = LeducCFR()
    start = time.time()
    value = solver.train(1_000_000)
    elapsed = time.time() - start

    print(f"Approximate game value for Player 1: {value:.6f}")
    print(f"Training time: {elapsed:.2f} seconds")

    strategy_json = {
        "game": "Simplified Leduc (6-card double-deck)",
        "notes": {
            "ante": 1,
            "bet_size": 1,
            "raise_cap_per_round": 1,
            "iterations": 1_000_000
        },
        "average_strategy": {}
    }

    for info_key in solver.strategy_sum:
        avg = solver.get_average_strategy(info_key)
        bet_pending = '_bp1' in info_key
        actions = ACTION_BET if bet_pending else ACTION_NO_BET
        strategy_json["average_strategy"][info_key] = {actions[i]: avg[i] for i in range(len(actions))}

    with open("leduc_strategy.json", "w") as f:
        json.dump(strategy_json, f, indent=2)

    print("Strategy saved to leduc_strategy.json")
