---
name: feedback_never_fabricate
description: NEVER fabricate numbers, timing data, or claims about code you haven't read. This has cost real money multiple times.
type: feedback
---

NEVER MAKE UP NUMBERS. NEVER CLAIM SOMETHING IS TRUE WITHOUT VERIFYING.

**Specific violations that cost real money and time:**
1. Called the search "MCTS" without reading the 74-line selfplay.rs. It was maximin. User spent a week and $400 on self-play based on this.
2. Wrote "~15-25ms" for 96ch CG inference in the PUCT prompt. Never measured. Made it up.
3. Wrote "~0.5ms" for CG inference elsewhere. That was M4 Max MPS timing, not CG.
4. Called self-play labels "MCTS labels" repeatedly. They're maximin labels.

**How to apply:**
- If you haven't READ the code, don't describe what it does
- If you haven't MEASURED a number, say "NOT MEASURED" and provide a way to measure it
- If you're unsure, say "I don't know" — never fill gaps with plausible-sounding bullshit
- Before using a technical term (MCTS, PUCT, etc.), verify the code actually implements that thing
- The user's money and time depend on accurate information. Fabrication is not a minor error.
