---
name: torque
trigger: glob
globs: src/**/*.tq
---

# Torque Rules

Rules for working with Torque files.

## Registration in BUILD.gn
- If you create a **new** `.tq` file, you **MUST** register it in the root `BUILD.gn`.
- Locate the `torque_files` list in `BUILD.gn` (Note: not `v8_torque_files`).
- Add your new file path in alphabetical order.
- *Note: Modifications to existing files do not require registration changes.*


