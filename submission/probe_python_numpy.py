import sys
import time

results = []

try:
    import numpy as np
    results.append(f"numpy={np.__version__}")
    a = np.random.randn(64, 64).astype(np.float32)
    b = np.random.randn(64, 966).astype(np.float32)
    t0 = time.time()
    for _ in range(100):
        c = a @ b
    dt = (time.time() - t0) * 1000
    results.append(f"matmul_64x64@64x966_100x={dt:.1f}ms ({dt/100:.2f}ms/call)")
except ImportError:
    results.append("numpy=NOT_AVAILABLE")

try:
    import scipy
    results.append(f"scipy={scipy.__version__}")
except ImportError:
    results.append("scipy=NOT_AVAILABLE")

try:
    import torch
    results.append(f"torch={torch.__version__}")
except ImportError:
    results.append("torch=NOT_AVAILABLE")

results.append(f"python={sys.version}")

for r in results:
    print(f"PROBE: {r}", file=sys.stderr)

# Init: player_index, width, height, grid rows, birds_per_player, my bird ids, opp bird ids
player_index = int(input())
width = int(input())
height = int(input())
for _ in range(height):
    input()
birds_per_player = int(input())
for _ in range(birds_per_player):
    input()
for _ in range(birds_per_player):
    input()

while True:
    try:
        apple_count = int(input())
        for _ in range(apple_count):
            input()
        bird_count = int(input())
        for _ in range(bird_count):
            input()
        print("WAIT")
        sys.stdout.flush()
    except EOFError:
        break
