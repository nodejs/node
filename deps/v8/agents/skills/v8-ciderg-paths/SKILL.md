---
name: v8-ciderg-paths
description: Adjusts instructions for V8 commands when running in a CiderG Chrome workspace environment. Trigger this when invoking any V8 binaries or running tests in a CiderG checkout.
---

# V8 CiderG Path Adjustments

Because CiderG nests the V8 repository as a component of a larger Chromium
checkout, the build output (`out/` directory) is generated at the root of the
Chromium checkout rather than the V8 directory.

## Path Rules

-   When executing from `chromium/v8`, the build directory is located one level
    up at `../out/`.
-   Adjust any relative binary or test runner paths accordingly.
    -   **Incorrect:** `out/x64.debug/d8`
    -   **Correct:** `../out/x64.debug/d8`
