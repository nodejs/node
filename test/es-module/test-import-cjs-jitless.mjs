// Flags: --jitless

// Tests that importing a CJS module works in JIT-less mode (i.e. falling back to the
// JS parser if WASM is not available).
import '../common/index.mjs';
import '../fixtures/empty.cjs';
