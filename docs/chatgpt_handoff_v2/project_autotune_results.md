---
name: torch.compile autotune results for 128ch teacher
description: max-autotune kernel benchmark results for 128ch/8-block SE-ResNet on H100 — use to skip 30min compile or optimize image caching
type: project
---

## torch.compile max-autotune Results (128ch/8-block, bs=4096, H100)

### Compile Time
- ~30 min on fresh container (no Triton cache)
- Benchmarks ~20 configs per kernel × hundreds of unique shapes
- One-time cost per model architecture + batch size

### Key Findings

**Convolutions (cuDNN wins over Triton ~2x):**
- `conv2d(4096×19×24×44, 128×19×3×3)`: cuDNN 1.45ms vs Triton 2.63ms (55%)
- `conv2d(4096×128×24×44, 128×128×3×3)`: cuDNN 1.78ms vs Triton 3.44ms (52%)
- Implication: channels_last would help cuDNN even more (if Muon .reshape() fork done)

**Matmuls (Triton wins):**
- SE squeeze `mm(4096×128, 128×32)`: Triton 0.006ms
- SE expand `mm(4096×32, 32×128)`: Triton 0.006ms
- Policy head `mm(4096×128, 128×134)`: Triton 0.008ms
- Value head `mm(4096×128, 128×20)`: Triton 0.007ms

**Backward pass transposed matmuls (split-K wins):**
- `mm(128×4096, 4096×32)`: decompose_k (k_split=32) 0.0097ms vs mm 0.0108ms
- `mm(20×4096, 4096×128)`: decompose_k (k_split=32) 0.0108ms vs mm 0.0110ms

### How to Speed Up Future Retraining

1. **Persist Triton cache in Modal image**: Set `TRITON_CACHE_DIR=/root/.triton` and bake into image after first run
2. **Use `mode="reduce-overhead"` instead**: Skips per-kernel benchmarking, compiles in ~2-5 min, ~90% of speedup
3. **Disable compile for short runs**: For 1-epoch tests, compile overhead (30 min) > training time savings
4. **Pre-warm approach**: Run 1 batch with compile, save cache, mount as volume

### Optimal Setting Per Run Length
- 1-2 epochs: `use_compile: false` (compile cost > benefit)
- 3-5 epochs: `mode="reduce-overhead"` (fast compile, good enough)
- 6+ epochs: `mode="max-autotune"` (amortized over many epochs)
