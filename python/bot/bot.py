"""
Python bot for CodinGame Winter Challenge 2026 (Exotec snake/bird game).
Single-file, self-contained. Uses NumPy for CNN inference (no PyTorch).
Weights are embedded as a constant string by generate_python_submission.py.
"""
from __future__ import annotations

import numpy as np

# ---------------------------------------------------------------------------
# Model configuration — set by submission generator to match trained model
# ---------------------------------------------------------------------------
MODEL_CONFIG = {
    "conv_channels": 16,
    "num_conv_layers": 3,
    "input_channels": 19,
    "scalar_features": 6,
    "birds_per_player": 4,
    "actions_per_bird": 5,
}

# Filled by generate_python_submission.py — f16 Unicode encoded weights
WEIGHTS_F16 = ""

# ---------------------------------------------------------------------------
# Constants (must match Rust features.rs)
# ---------------------------------------------------------------------------
HYBRID_GRID_CHANNELS = 19
SCALAR_FEATURES = 6
MAX_BOARD_HEIGHT = 24
MAX_BOARD_WIDTH = 44
MAX_BIRDS_PER_PLAYER = 4
POLICY_ACTIONS_PER_BIRD = 5

# Directions: index -> (dx, dy), matching policy_index_for_command in Rust
#   0=KEEP, 1=North(UP), 2=East(RIGHT), 3=South(DOWN), 4=West(LEFT)
DIR_DELTAS = [(0, 0), (0, -1), (1, 0), (0, 1), (-1, 0)]
DIR_NAMES = ["KEEP", "UP", "RIGHT", "DOWN", "LEFT"]

# Direction opposites: N<->S, E<->W
DIR_OPPOSITE = {1: 3, 3: 1, 2: 4, 4: 2}

# ---------------------------------------------------------------------------
# Weight decoding
# ---------------------------------------------------------------------------

def decode_weights(s: str) -> np.ndarray:
    """Decode f16 Unicode string (with surrogate escaping) to float32 array.

    Encoding uses U+FFFF sentinel to escape surrogates (0xD800..0xDFFF):
      - chr(0xFFFF) + chr(off)     -> 0xD800 + off  (if off != 0x0800)
      - chr(0xFFFF) + chr(0x0800)  -> 0xFFFF itself
    """
    raw_u16 = []
    i = 0
    while i < len(s):
        ch = ord(s[i])
        if ch == 0xFFFF:
            i += 1
            if i < len(s):
                off = ord(s[i])
                raw_u16.append(0xFFFF if off == 0x0800 else 0xD800 + off)
            i += 1
        else:
            raw_u16.append(ch)
            i += 1
    arr = np.array(raw_u16, dtype=np.uint16)
    return np.frombuffer(arr.tobytes(), dtype=np.float16).astype(np.float32)


def load_model(flat: np.ndarray, cfg: dict) -> dict:
    """Slice flat weight array into layer parameters based on config."""
    C = cfg["conv_channels"]
    Ci = cfg["input_channels"]
    S = cfg["scalar_features"]
    B = cfg["birds_per_player"]
    A = cfg["actions_per_bird"]
    n_layers = cfg["num_conv_layers"]

    layers = {}
    offset = 0

    def take(shape):
        nonlocal offset
        n = 1
        for s in shape:
            n *= s
        arr = flat[offset : offset + n].reshape(shape)
        offset += n
        return arr

    # conv1: weight (C, Ci, 3, 3), bias (C,)
    layers["conv1_w"] = take((C, Ci, 3, 3))
    layers["conv1_b"] = take((C,))
    # conv2: weight (C, C, 3, 3), bias (C,)
    layers["conv2_w"] = take((C, C, 3, 3))
    layers["conv2_b"] = take((C,))
    # conv3 (optional)
    if n_layers >= 3:
        layers["conv3_w"] = take((C, C, 3, 3))
        layers["conv3_b"] = take((C,))
    # policy_head: weight (B*A, C+S), bias (B*A,)
    layers["policy_w"] = take((B * A, C + S))
    layers["policy_b"] = take((B * A,))
    # value_head: weight (1, C+S), bias (1,)
    layers["value_w"] = take((1, C + S))
    layers["value_b"] = take((1,))

    assert offset == len(flat), f"Weight count mismatch: used {offset}, have {len(flat)}"
    return layers


# ---------------------------------------------------------------------------
# NumPy CNN Inference
# ---------------------------------------------------------------------------

def conv2d_relu(x: np.ndarray, w: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Conv2d(3x3, padding=1) + ReLU. x: (Ci,H,W), w: (Co,Ci,3,3), b: (Co,)."""
    Ci, H, W = x.shape
    Co = w.shape[0]
    # Pad input with zeros (padding=1)
    xp = np.zeros((Ci, H + 2, W + 2), dtype=np.float32)
    xp[:, 1 : H + 1, 1 : W + 1] = x
    # Accumulate convolution via 9 shifted matmuls
    out = np.zeros((Co, H, W), dtype=np.float32)
    for dy in range(3):
        for dx in range(3):
            patch = xp[:, dy : dy + H, dx : dx + W]  # (Ci, H, W)
            # w[:, :, dy, dx] is (Co, Ci) — dot with patch over Ci axis
            out += np.einsum("ij,jhw->ihw", w[:, :, dy, dx], patch)
    out += b[:, None, None]
    np.maximum(out, 0, out=out)  # ReLU in-place
    return out


def global_avg_pool(x: np.ndarray) -> np.ndarray:
    """x: (C, H, W) -> (C,)"""
    return x.mean(axis=(1, 2))


def linear(x: np.ndarray, w: np.ndarray, b: np.ndarray) -> np.ndarray:
    """x: (in_features,), w: (out_features, in_features), b: (out_features,)"""
    return w @ x + b


def forward(grid: np.ndarray, scalars: np.ndarray, layers: dict, cfg: dict) -> tuple:
    """Run TinyHybridNet forward pass. Returns (policy_logits[4,5], value)."""
    x = conv2d_relu(grid, layers["conv1_w"], layers["conv1_b"])
    x = conv2d_relu(x, layers["conv2_w"], layers["conv2_b"])
    if cfg["num_conv_layers"] >= 3 and "conv3_w" in layers:
        x = conv2d_relu(x, layers["conv3_w"], layers["conv3_b"])
    pooled = global_avg_pool(x)
    features = np.concatenate([pooled, scalars])
    policy_logits = linear(features, layers["policy_w"], layers["policy_b"])
    value_raw = linear(features, layers["value_w"], layers["value_b"])
    value = float(np.tanh(value_raw[0]))
    policy = policy_logits.reshape(cfg["birds_per_player"], cfg["actions_per_bird"])
    return policy, value


# ---------------------------------------------------------------------------
# I/O Parsing (matches Rust input.rs exactly)
# ---------------------------------------------------------------------------

def read_init():
    """Read init block. Returns (player_index, width, height, walls, my_ids, opp_ids)."""
    player_index = int(input())
    width = int(input())
    height = int(input())
    walls = set()
    for y in range(height):
        row = input()
        for x, ch in enumerate(row):
            if ch == "#":
                walls.add((x, y))
    birds_per_player = int(input())
    my_ids = [int(input()) for _ in range(birds_per_player)]
    opp_ids = [int(input()) for _ in range(birds_per_player)]
    return player_index, width, height, walls, my_ids, opp_ids


def read_frame():
    """Read per-turn frame. Returns (apples, live_birds) or None at EOF.
    live_birds: dict of bird_id -> list of (x,y) segments, head first.
    """
    try:
        line = input()
    except EOFError:
        return None
    apple_count = int(line)
    apples = []
    for _ in range(apple_count):
        parts = input().split()
        apples.append((int(parts[0]), int(parts[1])))
    live_bird_count = int(input())
    live_birds = {}
    for _ in range(live_bird_count):
        parts = input().split()
        bird_id = int(parts[0])
        body_str = parts[1]
        segments = []
        for seg in body_str.split(":"):
            xy = seg.split(",")
            segments.append((int(xy[0]), int(xy[1])))
        live_birds[bird_id] = segments
    return apples, live_birds


# ---------------------------------------------------------------------------
# Feature Encoding (matches Rust features.rs::encode_hybrid_position exactly)
# ---------------------------------------------------------------------------

def canonical_coord(x: int, y: int, player_index: int, width: int, height: int) -> tuple:
    """Mirror coordinates for player 1. Matches Rust Grid::opposite (horizontal only)."""
    if player_index == 0:
        return (x, y)
    return (width - 1 - x, y)


def segment_has_support(
    x: int,
    y: int,
    bird_id: int,
    walls: set,
    apples: list,
    live_birds: dict,
    height: int,
) -> bool:
    """Check if segment at (x,y) has support from below.
    Supported if the cell directly below has: wall, apple, or another bird's body.
    Matches Rust features.rs::segment_has_support.
    """
    bx, by = x, y + 1  # below
    if by >= height:
        return False  # off the bottom edge -> no support
    if (bx, by) in walls:
        return True
    if (bx, by) in apples:
        return True
    # Check other birds' bodies
    for other_id, segments in live_birds.items():
        if other_id == bird_id:
            continue
        if (bx, by) in segments:
            return True
    return False


def bird_slot_ids(my_ids: list, opp_ids: list, owner_is_me: bool) -> list:
    """Get sorted bird IDs for a player, truncated to MAX_BIRDS_PER_PLAYER.
    Matches Rust bird_slot_ids: collects all birds for owner, sorts by id.
    """
    ids = list(my_ids if owner_is_me else opp_ids)
    ids.sort()
    return ids[:MAX_BIRDS_PER_PLAYER]


def get_facing(segments: list) -> int:
    """Determine facing direction index from body segments.
    Returns direction index (1=N, 2=E, 3=S, 4=W) or 0 for Unset.
    Matches Rust BirdState::facing.
    """
    if len(segments) < 2:
        return 0  # Unset
    hx, hy = segments[0]
    nx, ny = segments[1]
    dx, dy = hx - nx, hy - ny
    for i in range(1, 5):
        if DIR_DELTAS[i] == (dx, dy):
            return i
    return 0  # Unset


def legal_command_count(segments: list) -> int:
    """Count legal commands for an alive bird.
    Matches Rust legal_commands_for_bird: KEEP + directions excluding
    current facing and its opposite.
    """
    facing = get_facing(segments)
    count = 1  # KEEP is always legal
    for d in range(1, 5):  # N, E, S, W
        if facing != 0 and d == DIR_OPPOSITE.get(facing, -1):
            continue
        if d == facing:
            continue
        count += 1
    return count


def encode_features(
    player_index: int,
    width: int,
    height: int,
    walls: set,
    apples: list,
    live_birds: dict,
    my_ids: list,
    opp_ids: list,
    turn: int,
    losses: list,
) -> tuple:
    """Encode game state into grid tensor and scalar vector.
    Returns (grid[19, MAX_H, MAX_W], scalars[6]).
    Matches Rust encode_hybrid_position + scalar_features exactly.
    """
    grid = np.zeros((HYBRID_GRID_CHANNELS, MAX_BOARD_HEIGHT, MAX_BOARD_WIDTH), dtype=np.float32)

    # Precompute apple set for fast lookup
    apple_set = set(apples)

    # Channel 0: walls
    for wx, wy in walls:
        cx, cy = canonical_coord(wx, wy, player_index, width, height)
        if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
            grid[0, cy, cx] = 1.0

    # Channel 1: apples
    for ax, ay in apples:
        cx, cy = canonical_coord(ax, ay, player_index, width, height)
        if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
            grid[1, cy, cx] = 1.0

    # Channel 2: unsupported segments (any alive bird segment without support)
    # Uses set-based lookup for other birds
    for bird_id, segments in live_birds.items():
        seg_set = set(segments)
        for sx, sy in segments:
            if not segment_has_support(sx, sy, bird_id, walls, apple_set, live_birds, height):
                cx, cy = canonical_coord(sx, sy, player_index, width, height)
                if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
                    grid[2, cy, cx] = 1.0

    # Channels 3-10: own birds (4 slots x 2 channels: head + body)
    own_slot_ids = bird_slot_ids(my_ids, opp_ids, owner_is_me=True)
    for slot_idx, bird_id in enumerate(own_slot_ids):
        head_ch = 3 + slot_idx * 2
        body_ch = head_ch + 1
        if bird_id in live_birds:
            segments = live_birds[bird_id]
            # Head
            hx, hy = segments[0]
            cx, cy = canonical_coord(hx, hy, player_index, width, height)
            if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
                grid[head_ch, cy, cx] = 1.0
            # Body (skip head)
            for sx, sy in segments[1:]:
                cx, cy = canonical_coord(sx, sy, player_index, width, height)
                if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
                    grid[body_ch, cy, cx] = 1.0

    # Channels 11-18: opponent birds (4 slots x 2 channels: head + body)
    opp_slot_ids = bird_slot_ids(my_ids, opp_ids, owner_is_me=False)
    for slot_idx, bird_id in enumerate(opp_slot_ids):
        head_ch = 11 + slot_idx * 2
        body_ch = head_ch + 1
        if bird_id in live_birds:
            segments = live_birds[bird_id]
            # Head
            hx, hy = segments[0]
            cx, cy = canonical_coord(hx, hy, player_index, width, height)
            if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
                grid[head_ch, cy, cx] = 1.0
            # Body (skip head)
            for sx, sy in segments[1:]:
                cx, cy = canonical_coord(sx, sy, player_index, width, height)
                if 0 <= cy < MAX_BOARD_HEIGHT and 0 <= cx < MAX_BOARD_WIDTH:
                    grid[body_ch, cy, cx] = 1.0

    # Scalar features (matches Rust scalar_features)
    own_body_score = 0
    opp_body_score = 0
    own_alive = 0
    opp_alive = 0
    own_breakpoints = 0
    opp_breakpoints = 0
    own_mobility = 0
    opp_mobility = 0

    for bird_id in my_ids:
        if bird_id in live_birds:
            segs = live_birds[bird_id]
            own_body_score += len(segs)
            own_alive += 1
            if len(segs) >= 4:
                own_breakpoints += 1
            own_mobility += legal_command_count(segs)

    for bird_id in opp_ids:
        if bird_id in live_birds:
            segs = live_birds[bird_id]
            opp_body_score += len(segs)
            opp_alive += 1
            if len(segs) >= 4:
                opp_breakpoints += 1
            opp_mobility += legal_command_count(segs)

    own_losses = losses[0]
    opp_losses = losses[1]

    scalars = np.array(
        [
            min(max(turn / 200.0, 0.0), 1.0),  # turn_progress
            min(max((own_body_score - opp_body_score) / 32.0, -1.0), 1.0),  # body_diff
            min(max((opp_losses - own_losses) / 32.0, -1.0), 1.0),  # loss_diff
            min(max((own_alive - opp_alive) / 4.0, -1.0), 1.0),  # live_diff
            min(max((own_breakpoints - opp_breakpoints) / 4.0, -1.0), 1.0),  # breakpoint_diff
            min(max((own_mobility - opp_mobility) / 16.0, -1.0), 1.0),  # mobility_diff
        ],
        dtype=np.float32,
    )

    return grid, scalars


# ---------------------------------------------------------------------------
# Action selection and output formatting
# ---------------------------------------------------------------------------

def select_actions(
    policy: np.ndarray,
    my_ids: list,
    live_birds: dict,
) -> str:
    """Select actions from policy logits. Returns CG output string.
    policy: (4, 5) — 4 bird slots, 5 actions each.
    Matches Rust render_action output format.
    """
    own_slot_ids = sorted(my_ids)[:MAX_BIRDS_PER_PLAYER]
    commands = []
    for slot_idx, bird_id in enumerate(own_slot_ids):
        if bird_id not in live_birds:
            continue
        logits = policy[slot_idx]
        action_idx = int(np.argmax(logits))
        if action_idx == 0:
            # KEEP — omit from output (matches Rust render_action)
            continue
        direction = DIR_NAMES[action_idx]
        commands.append(f"{bird_id} {direction}")

    if not commands:
        return "WAIT"
    commands.sort()
    return ";".join(commands)


# ---------------------------------------------------------------------------
# Loss tracking via dead bird counting
# ---------------------------------------------------------------------------

def update_losses(
    losses: list,
    my_ids: list,
    opp_ids: list,
    live_birds: dict,
    prev_live_birds: dict | None,
) -> None:
    """Update loss counters by detecting newly dead birds.
    A bird that was alive last turn but absent this turn counts as a new loss.
    """
    if prev_live_birds is None:
        return
    for bird_id in my_ids:
        if bird_id in prev_live_birds and bird_id not in live_birds:
            losses[0] += 1
    for bird_id in opp_ids:
        if bird_id in prev_live_birds and bird_id not in live_birds:
            losses[1] += 1


# ---------------------------------------------------------------------------
# Main game loop
# ---------------------------------------------------------------------------

def main():
    # Read init
    player_index, width, height, walls, my_ids, opp_ids = read_init()

    # Load model weights
    layers = None
    if WEIGHTS_F16:
        flat_weights = decode_weights(WEIGHTS_F16)
        layers = load_model(flat_weights, MODEL_CONFIG)

    # Game state tracking
    losses = [0, 0]
    turn = 0
    prev_live_birds = None

    # Game loop
    while True:
        frame = read_frame()
        if frame is None:
            break
        apples, live_birds = frame

        # Track losses
        update_losses(losses, my_ids, opp_ids, live_birds, prev_live_birds)

        if layers is not None:
            # Encode features
            grid, scalars = encode_features(
                player_index, width, height, walls, apples, live_birds,
                my_ids, opp_ids, turn, losses,
            )
            # Run inference
            policy, value = forward(grid, scalars, layers, MODEL_CONFIG)
            # Select and output actions
            output = select_actions(policy, my_ids, live_birds)
        else:
            # Fallback: no model loaded, just WAIT
            output = "WAIT"

        print(output, flush=True)

        prev_live_birds = live_birds
        turn += 1


if __name__ == "__main__":
    main()
