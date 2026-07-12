import { strictEqual } from 'node:assert';

// Pre-load simple.wasm at kSourcePhase to prime the loadCache.
const preloaded = await import.source('./simple.wasm');
strictEqual(preloaded instanceof WebAssembly.Module, true);

// Import a parent that has both eval-phase and source-phase imports of the
// same wasm file, which triggers ensurePhase(kEvaluationPhase) on the cached
// job and exposes the loadCache eviction bug.
const { mod1, mod2, mod3, mod4 } =
  await import('./test-wasm-source-phase-identity-parent.mjs');

strictEqual(mod1, mod2, 'two eval-phase imports of the same wasm must be identical');
strictEqual(mod3, mod4, 'two source-phase imports of the same wasm must be identical');
