import assert from 'node:assert';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import { dirname } from 'node:path';

// Test createRequire with process.execPath.
const assert2 = createRequire(process.execPath)('node:assert');
assert.strictEqual(assert2.strict, assert.strict);

// Test import.meta properties.
assert.strictEqual(import.meta.url, pathToFileURL(process.execPath).href);
assert.strictEqual(import.meta.filename, process.execPath);
assert.strictEqual(import.meta.dirname, dirname(process.execPath));
assert.strictEqual(import.meta.main, true);

// Test import() with a built-in module.
const { strict } = await import('node:assert');
assert.strictEqual(strict, assert.strict);

console.log('ESM SEA with code cache executed successfully');
