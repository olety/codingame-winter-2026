# PUCT Search Implementation for SNAKEBYTE

## Context

SNAKEBYTE is a 2-player simultaneous-move snake game on CodinGame (WinterChallenge 2026). Each player controls 2-4 birds (snakes). Each bird can: keep going straight, turn left, or turn right — 3 options per bird. A player's joint action is the combination of all their birds' commands.

**Branching factor per player:** 3^N where N = alive birds (typically 2-4).
- 4 birds: 81 actions/player, 6,561 joint pairs
- 3 birds: 27 actions/player, 729 joint pairs
- 2 birds: 9 actions/player, 81 joint pairs

**Current search:** Maximin with alpha-beta. Evaluates ALL root pairs exhaustively (6,561 at 4 birds), then selectively deepens top moves. Budget: ~40ms per turn. This wastes 95% of the node budget on garbage moves.

**Student neural network:** `TinyHybridNet` — 3-layer CNN (96 channels, int4 quantized).
- Input: 19-channel binary grid (24×44) + 6 scalar features
- Output: policy logits (4 actions: per-bird directions mapped to 5 classes each) + scalar value
- Policy accuracy: ~78% against teacher
- The policy output is per-bird (4 birds × 5 classes), NOT over the 81 joint actions directly

**Goal:** Replace maximin with PUCT in the C bot submission. Same student weights. Must fit in 100K chars total (code + weights). Current code is ~45K chars, weights ~47K chars, leaving ~8K chars of headroom.

## Game Mechanics Summary

- 2 players, simultaneous moves each turn
- Each player has 2-4 birds (snakes) on a 2D grid with walls
- Birds move forward each turn; players can turn birds left/right
- Birds die on collision (wall, other bird, self). Eating apples grows the bird.
- Game ends at turn 200 or when all birds of one player die
- Score = total length of alive birds. Higher score wins.
- Maps are vertically symmetric (player 0 top half, player 1 bottom half)

## Architecture Decision: How to Handle Simultaneous Moves

Three main approaches for MCTS in simultaneous-move games:

### Option A: Decoupled PUCT (RECOMMENDED)
- Our player selects actions via PUCT (exploring promising moves)
- For the opponent at each simulation, assume they play their best response (minimax) or sample from a distribution
- Tree only tracks OUR actions. At each node, we pick our action, then simulate opponent's response.
- **Pro:** Simple tree structure, policy prior maps directly to our action selection
- **Con:** Doesn't model opponent uncertainty well

### Option B: Sequential PUCT (Max node → Min node)
- Alternate layers: our nodes (maximize) and opponent nodes (minimize)
- Our nodes use PUCT with our policy prior
- Opponent nodes use PUCT with inverted value (or uniform prior)
- Like Alpha-Beta but with PUCT selection
- **Pro:** Naturally handles the adversarial structure
- **Con:** Doubles the tree depth, policy prior only useful at our nodes

### Option C: Matrix PUCT (Simultaneous)
- Each node stores an |A_me| × |A_opp| matrix of visit counts
- Selection uses something like O-MCTS or regret matching
- **Pro:** Game-theoretically sound
- **Con:** Complex, high memory, hard to integrate policy prior

**Recommendation: Option B (Sequential PUCT).** It maps naturally to maximin, the policy prior guides our action selection, and the opponent modeling is sound. The value head evaluates from our perspective at leaf nodes.

## PUCT Algorithm

At each node during selection, pick the action `a` that maximizes:

```
PUCT(s, a) = Q(s, a) + c_puct * P(s, a) * sqrt(N(s)) / (1 + N(s, a))
```

Where:
- `Q(s, a)` = mean value of action a = W(s,a) / N(s,a), or 0 if N(s,a)=0
- `P(s, a)` = policy prior from student network (softmax over actions)
- `N(s)` = total visits to node s = sum of N(s,a) for all a
- `N(s, a)` = visit count for action a at node s
- `c_puct` = exploration constant (start with 1.0, tune between 0.5-2.0)

For OPPONENT nodes (minimizing), pick action that minimizes:
```
PUCT_opp(s, a) = Q(s, a) - c_puct * P_opp(s, a) * sqrt(N(s)) / (1 + N(s, a))
```
Or simply: opponent picks action that minimizes Q, with exploration bonus to avoid being too deterministic. Can use uniform prior for opponent (1/num_actions) since we don't have a model of their policy, or mirror the student as opponent prior.

## Converting Per-Bird Policy to Joint Action Prior

The student outputs per-bird logits: `policy[4][5]` where each bird has 5 classes. But PUCT needs a prior over the 81 joint actions.

**Approach:** Factor the joint probability as product of independent per-bird probabilities:
```
P(joint_action) = P(bird0_cmd) * P(bird1_cmd) * P(bird2_cmd) * P(bird3_cmd)
```

For each bird, map the 5-class output to the 3 legal commands (keep/left/right). The 5 classes correspond to directions (N/E/S/W/none or similar) — need to map based on current facing direction:
- Class matching current facing → "keep" (go straight)
- Class matching left of facing → "turn left"
- Class matching right of facing → "turn right"
- Other classes → redistribute probability

After softmax on each bird's logits and mapping to 3 commands, multiply across birds for each of the 81 joint actions. Then normalize.

## Tree Structure (C implementation)

```c
#define MAX_NODES 50000  // tune based on memory/time budget
#define MAX_ACTIONS 81

typedef struct {
    int parent;           // parent node index (-1 for root)
    int parent_action;    // action that led here
    int children[MAX_ACTIONS]; // child node index per action (-1 if unexpanded)
    int num_actions;      // legal actions at this node
    float prior[MAX_ACTIONS]; // P(s,a) from policy network
    int visit_count[MAX_ACTIONS]; // N(s,a)
    float total_value[MAX_ACTIONS]; // W(s,a)
    int total_visits;     // N(s) = sum of visit_count
    int is_our_turn;      // 1 = max node (our turn), 0 = min node (opponent)
} MCTSNode;

MCTSNode tree[MAX_NODES];
int num_nodes;
```

## PUCT Search Loop

```
function puct_search(root_state, time_budget_ms):
    create root node with our policy prior

    while time remaining:
        // 1. SELECT: walk down tree picking PUCT-best actions
        node = root
        state = root_state.clone()
        path = []

        while node is fully expanded and not terminal:
            if node.is_our_turn:
                action = argmax_a PUCT(node, a)
            else:
                action = argmin_a PUCT_opp(node, a) // or argmin Q with exploration
            path.append((node, action))
            state.apply(action)  // for sequential: apply one player's action
            node = node.children[action]

        // 2. EXPAND: add a new child node
        if not terminal:
            action = pick unexpanded action with highest prior
            apply action to state
            child = create_node(state)
            if child.is_our_turn:
                run student network on state → set child.prior from policy
            else:
                set child.prior to uniform or mirror policy
            node.children[action] = child
            path.append((node, action))

        // 3. EVALUATE: use student value head
        value = student_value(state)  // from our perspective

        // 4. BACKPROP: update W and N along path
        for (node, action) in reversed(path):
            node.visit_count[action] += 1
            node.total_visits += 1
            node.total_value[action] += value

    // Pick most-visited action at root
    best = argmax_a root.visit_count[a]
    return best
```

## Key Implementation Notes

1. **State cloning:** Each simulation needs a copy of the game state to modify. The state is a grid + bird positions. Keep it compact.

2. **NN inference during search:** Running the CNN at every leaf is expensive (~0.5ms on CG hardware for 96ch). With 40ms budget, that's ~80 evaluations max. This is LOW. Options:
   - Only evaluate with NN at root and depth 1, use heuristic eval deeper
   - Cache NN evaluations in a transposition table
   - Use the NN only for the prior at root, use heuristic value for leaf evaluation
   - **Hybrid approach (recommended):** NN prior at root + NN value at depth 1 leaves + heuristic eval at depth 2+

3. **Simultaneous to sequential:** At each game turn, the tree has two levels: our action (max) then opponent action (min). After both are selected, `state.step(our_action, opp_action)` advances the game. So tree depth 2 = 1 game turn.

4. **Memory budget:** 50K nodes × ~400 bytes/node = 20MB. Should be fine for CG.

5. **Time management:** First turn gets 850ms (can do ~1700 NN evals or ~400K heuristic evals). Later turns get 40ms (~80 NN evals). The search should adapt — use NN value for leaves on turn 0, switch to heuristic eval on later turns for more iterations.

6. **c_puct tuning:** With 78% policy accuracy, the prior is strong. Lower c_puct (0.5-1.0) to exploit the prior more. Higher c_puct (1.5-2.5) for more exploration. Start at 1.0.

7. **Temperature for action selection:** At root, can use visit count directly (pick most visited) or with temperature. For competitive play, use tau=0 (argmax visits).

8. **Virtual loss:** If implementing with multiple threads (not applicable for CG single-thread), add virtual loss during selection to encourage exploration.

## Comparison: Current Maximin vs PUCT

| Aspect | Current Maximin | PUCT |
|--------|----------------|------|
| Root evaluation | All 6,561 pairs | Focuses on top ~10 actions via prior |
| Effective depth | 1.75 plies | 4-8 plies on promising lines |
| Node budget usage | 95% on garbage moves | 80% on top moves |
| NN usage | Heuristic + leaf_mix blend | Policy prior + value evaluation |
| Opponent model | Worst-case (minimax) | Explored via PUCT (can be minimax) |
| Implementation | Simple nested loops | Tree + selection + backprop |

## Files to Modify

- `c_bot/bot.c` — add PUCT search, replace maximin `choose_action`
- OR `rust/bot/src/search.rs` — implement in Rust first for testing, then port to C
- `tools/generate_c_submission.py` — may need to adjust character budget

## Testing Strategy

1. Implement PUCT in Rust first (easier to debug, test against Java referee)
2. Run arena: PUCT bot vs maximin bot on 100+ seeds
3. Tune c_puct for highest win rate
4. Port to C bot
5. Verify fits in 100K chars
6. Test on CG timing (must complete within 40ms/turn)

## Open Questions

- Should opponent nodes use uniform prior, mirror our policy, or something else?
- At 40ms with ~0.5ms NN inference, we get ~80 iterations. Is that enough for PUCT to converge? May need to reduce NN calls (root-only prior, heuristic leaf eval).
- With int4 quantized 96ch model, what's the actual inference time on CG hardware? Previous timing probes showed ~15-25ms for the NN alone — if so, we can only do 1-2 NN evals per turn, which means PUCT with NN leaf eval won't work. May need policy-prior-only PUCT with heuristic leaf eval.
- Character budget: PUCT tree code adds ~5-8K chars. Current budget has ~8K headroom. Tight but possible.
