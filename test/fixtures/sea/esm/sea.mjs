import assert from 'node:assert';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import { dirname } from 'node:path';

// Test createRequire with process.execPath.
const assert2 = createRequire(process.execPath)('node:assert');
assert.strictEqual(assert2.strict, assert.strict);

// Test import.meta properties. This should be in sync with the CommonJS entry
// point's corresponding values.
assert.strictEqual(import.meta.url, pathToFileURL(process.execPath).href);
assert.strictEqual(import.meta.filename, process.execPath);
assert.strictEqual(import.meta.dirname, dirname(process.execPath));
assert.strictEqual(import.meta.main, true);
// TODO(joyeecheung): support import.meta.resolve when we also support
// require.resolve in CommonJS entry points, the behavior of the two
// should be in sync.

// Test import() with a built-in module.
const { strict } = await import('node:assert');
assert.strictEqual(strict, assert.strict);

console.log('ESM SEA executed successfully');
