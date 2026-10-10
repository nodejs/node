// Flags: --experimental-loader ./test/fixtures/es-module-loaders/non-file-cjs-source.mjs
import '../common/index.mjs';
import assert from 'node:assert';

// When a load hook provides the source of a CommonJS module, the `require` calls
// that source makes are resolved by the ESM loader, so registered hooks apply to
// them. That has to keep working when the module has a URL that is not a file:
// the CJS resolver cannot resolve a relative specifier against such a referrer.

const { default: fromHookedRequire } = await import('custom:entry');
assert.strictEqual(fromHookedRequire, 'loaded through the hooks');

// When the hooks do not claim the specifier either, the failure has to come from
// the ESM resolver rather than from the CJS one.
await assert.rejects(import('custom:missing-dep'), {
  code: 'ERR_UNSUPPORTED_RESOLVE_REQUEST',
});
