# BPS — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### `verifyFileSizeAdvisory` warning is dead code in practice

Tracing `verifySource` in `app/Main.hs:694-714`:

1. `checkCRC` runs first. Without `--no-verify`, CRC mismatch calls `die`.
2. The `unless noVerify $ do` block — which contains the advisory warning — is skipped when `--no-verify` *is* set.

So the advisory warning only fires when (a) CRC passes or is absent, (b) file size disagrees, and (c) `--no-verify` is not set. For any format with a CRC over the whole source (BPS, UPS, NINJA1, and others), (a) and (b) almost never hold simultaneously — a file of the wrong size CRCs wrong too, and `checkCRC` dies first.

The advisory was designed to add a specific diagnostic on top of the CRC gate. Current code ordering means it adds nothing in the common path.

Open question: revive it, or remove it?

- **Revive**: the "your file is N bytes but the patch declares M" message is genuinely sharper than a CRC mismatch. Options: (i) run the advisory check *before* `checkCRC` so its warning fires regardless of what the CRC does next, or (ii) bundle the file-size detail into the CRC error message itself ("source CRC mismatch — note also that your file is N bytes but the patch declared M").
- **Remove**: if we don't plumb a pre-CRC warning, the advisory is clutter; everything it's supposed to provide is subsumed by the CRC fatal.

Slap-wide concern, not BPS-specific. Every format populating `verifyFileSizeAdvisory` inherits the same dead-code outcome.
