# NINJA1 — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### slap creates only the binary subformat

The four subformats are all creatable in principle — a textual body is the same records in hex — but only the binary shapes are made so far. Noted so the gap reads as a choice not yet made, not an oversight.

### The zero-skip sentinel arrived in v1.01

The tool's changelog notes the zero-skip rule "was in the specs but mistakenly passed over" in v1.0, and v1.01 taught binary patching to read a zero-valued CRC32/MD5/SHA1 as "no known" value. The rule was always in the spec; the tool grew into it in its second release. slap reads a zero field as absent, following v1.01.

### The reference diffs large sources raw, whatever their ROM type

In `ninja.php`, the 30 MiB sampling rule that shrinks the verification hash also sends creation down a streaming path that diffs raw — a 40 MiB SNES source is compared as raw bytes even under the `snes` label, the header-stripping running only on the smaller-file path. It falls out of how that path is built, not anything the spec sheet describes. Whether NINJA1 creation should follow the same large-source-diffs-raw behavior, or normalize at every size, is open. Noted, not a commitment either way.
