---
name: submission encoding and character budget
description: CodinGame 100K character limit strategy — base64 to Unicode migration, character budget math for different model sizes
type: project
---

## CodinGame 100K Character Limit

The limit is on **characters, not bytes**. Rust source uses UTF-8, but CG counts Unicode codepoints.

### Encoding comparison

| Encoding | Chars per weight | 5K params | 15K params | 20K params | 26K params |
|----------|-----------------|-----------|-----------|-----------|-----------|
| Base64 (f32) | 5.33 | 27K | 81K | 107K | 139K |
| Unicode BMP (f32) | ~2.0 | ~10K | ~30K | ~40K | ~52K |
| **Unicode BMP (f16)** | **~1.0** | **~5K** | **~15K** | **~20K** | **~26K** |

### Character budget (with ~65K code after compaction + optimized conv)

| Architecture | Params | f16 Unicode chars | Total | Headroom | Fits? |
|---|---|---|---|---|---|
| 12ch/3L | 5,079 | ~5K | 70K | 30K | Yes |
| 16ch/3L | 7,875 | ~8K | 73K | 27K | Yes |
| 24ch/3L | 15,195 | ~15K | 81K | 19K | Yes |
| **28ch/3L** | **19,719** | **~20K** | **86K** | **14K** | **Yes** |
| **32ch/3L** | **25,971** | **~26K** | **91K** | **9K** | **Yes** |
| 48ch/3L | 54,195 | ~54K | 117K | - | No |

### Compaction history

- Original uncompacted: ~236K chars
- After comment/blank strip + indent compaction (4->1 space): 89K chars (with base64)
- After Unicode f32 encoding (base64->BMP 2-byte): 72K chars (12ch)
- After f16 quantization (f32->f16 + Unicode BMP): 70K chars (12ch)
- f16 enables 32ch/3L (91K) within 100K limit

### Encoding implementation

- Python encoder: `_encode_weights_f16_unicode()` in `tools/generate_flattened_submission.py`
- Rust decoder: `decode_f16_weights()` + `f16_to_f32()` bit manipulation
- Surrogate range D800-DFFF escaped using U+FFFF sentinel
- f16 has ~3 decimal digits precision — negligible accuracy loss for NN weights

**Why:** The 100K char limit is the binding constraint on model size. f16 + Unicode is ~5.3x denser than base64, unlocking 32ch/3L models that wouldn't fit otherwise.
**How to apply:** When evaluating new model sizes, check the budget table above. 32ch/3L is the practical maximum at 91K chars.
