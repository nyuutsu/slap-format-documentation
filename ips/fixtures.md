# IPS — test fixture inventory

All paths relative to repo root. Sizes in bytes. Provenance is from
the suite files (`test/suites/*.suite`) — "converted" means re-diffed
by an external tool, "real" means authentic historical patch,
"synthetic" is a suite-file-declared tier for heavy-diff patches that
are neither real-world nor round-tripped from another format.

## StandardIPS (`PATCH` / `EOF`)

| File | Size | Provenance | Notes |
|---|---|---|---|
| `test/data/dm4y/patch.ips` | 560,669 | converted (Flips) | 4 MB GBC base (`dm4y/base.gbc`). Primary small-ROM fixture; dm4y.suite exercises 14 formats including this. |
| `test/data/emerald/heavy-diff/patch.ips` | 6,702,378 | synthetic (Flips) | 16 MB GBA base (`emerald/base.gba`). Exercises ≤16 MB ceiling. Also the cross-validation fixture (`test/specs/crossval.txt:25`: `ips \| emerald-heavy \| ... \| flips`). |
| `test/data/fe6/fe6.ips` | 610,179 | real | Fire Emblem 6 English translation. Standalone real-world patch; `test/suites/fe6-ips.suite`. Only "real" StandardIPS fixture in the tree. |
| `test/data/paper-mario/deblur.ips` | 207 | real | N64 deblur, 207 bytes total, cross-validated against the paired APS. Exercises the tiny-patch path. |

## IPS32 (`IPS32` / `EEOF`)

| File | Size | Provenance | Notes |
|---|---|---|---|
| `test/data/dm4y/patch.ips32` | 760,118 | converted (sips) | Same source ROM as `patch.ips`. Exercises the IPS32 parser on a small input. |
| `test/data/stadium2/heavy-diff/patch.ips32` | 27,360,744 | synthetic (sips) | 64 MB N64 base. Only >16 MB fixture. This is the stress test for the stadium2-scale action-stream memory claims from the BPS docstring. |

## EBP (IPS with trailing JSON)

| File | Size | Provenance | Notes |
|---|---|---|---|
| `test/data/dm4y/patch.ebp` | 748,501 | converted (RomPatcher.js) | Searched with `grep -ao '"patcher"'`: zero matches. This fixture has **no JSON metadata** — it's an IPS (with "EOF" trailer) that happens to be named `.ebp`, from a converter that didn't emit a metadata block. Real-world edge case for the parser: `.ebp` extension, no JSON. |
| `test/data/emerald/heavy-diff/patch.ebp` | 6,702,461 | synthetic (RomPatcher.js) | Has JSON: trailer ends with `..."description":"Generated for slap testing"}`. Exercises the full EBP code path including metadata round-trip. Size difference vs. the sibling `.ips` is 83 bytes = exactly the JSON blob. |

## Sentinel-collision and truncation property fixtures

Not files on disk — generated inside the test suite:

- `test/Props/RoundTrip.hs:161-169` — `prop_ipsEofCollision`, QuickCheck-driven source/target pairs that exercise the `0x454F46` sentinel path end-to-end.
- `test/Props/RoundTrip.hs:171-187` — `prop_avoidSentinel` unit-style table of `avoidSentinel` cases.
- `test/Props/Contracts.hs:131-180` — `prop_ipsSentinelDirect`, `prop_ips32SentinelDirect`, `prop_ipsSentinelSplitDirect`, `prop_ips32SentinelSplitDirect`, `prop_ipsSentinelWithSource`. These are the contract-system tests for direct-conversion sentinel rejection.
- `test/Props/Truncation.hs:69-85` — `prop_ipsTrunc`, `prop_ips32Trunc`, `prop_ebpTrunc`: fuzz the parser against arbitrarily truncated inputs. Generic robustness, not variant-specific.

## Integration / CLI test shards

- `test/Integration/CLI.hs:38` — `dm4yIps = repo </> "test/data/dm4y/patch.ips"` threaded through the CLI round-trip suite.
- `test/Integration/CLI.hs:84-85, 270, 277, 284` — hand-crafted tiny IPS byte sequences (`PATCH\x01\x02` for truncated-input warnings; `PATCHEOF` for the empty-patch case).
- `test/Integration/FailureMode.hs:299-310` — `round-trip/IPS -> EBP -> IPS` and `round-trip/IPS -> PPF3 -> IPS` conversion chains.
- `test/Integration/FailureMode.hs:397, 409` — `create-round-trip/dm4y IPS` and `create-round-trip/stadium2 IPS32`.

## Coverage gaps

- No real-world `.ips32` or `.ebp` fixture — both variants are tooling-converted or synthetic. The rewrite cannot regress against a known-real-world IPS32 pattern because none exists in the tree.
- No fixture exercises an IPS with Flips-emitted truncation marker. `prop_ipsTrunc` fuzzes truncation but doesn't specifically produce a shrink-direction patch. Worth adding a unit test with a known Flips-produced truncation patch if one is available externally.
- No fixture exercises an EBP with a truncation marker in the "slap-only extension" position (truncate-then-JSON). This makes dropping the writer side of that shape safe.
