// Flags: --experimental-import-meta-resolve
import '../common/index.mjs';
import assert from 'node:assert';
import { registerHooks } from 'node:module';

// Asserts that import.meta.resolve invokes loader hooks registered via `registerHooks`.

registerHooks({
  resolve(specifier, context, defaultResolve) {
    if (specifier === 'custom:hooked') {
      return {
        shortCircuit: true,
        url: new URL('./test-esm-import-meta.mjs', import.meta.url).href,
      };
    }
    return defaultResolve(specifier, context);
  },
});

assert.strictEqual(import.meta.resolve('custom:hooked'),
                   new URL('./test-esm-import-meta.mjs', import.meta.url).href);
