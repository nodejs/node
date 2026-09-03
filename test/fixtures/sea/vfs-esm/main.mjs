// The ESM SEA main script runs from inside the VFS mount: import.meta
// reflects the mount, and static imports, dynamic imports, and bare
// specifier lookups all resolve against the bundled assets.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert';
import { createRequire } from 'node:module';

// Static import of a relative ESM module from the VFS.
import { add, multiply } from './modules/math.mjs';

// Static import of a CommonJS module from the VFS (interop).
import calculator from './modules/calculator.cjs';

// Static import of a bare specifier resolved via the in-VFS node_modules.
import { name as pkgName, greet } from 'test-esm-pkg';

// The main script runs from inside the VFS, not from the executable.
assert.ok(import.meta.url.startsWith('file:'));
assert.strictEqual(path.basename(import.meta.filename), 'main.mjs');
assert.notStrictEqual(import.meta.filename, process.execPath);
console.log('main module runs from', import.meta.url);

// import.meta.dirname is the root of the mounted assets.
const config = JSON.parse(
  fs.readFileSync(path.join(import.meta.dirname, 'config.json'), 'utf8'));
assert.strictEqual(config.name, 'test-app');
const greeting = fs.readFileSync(
  path.join(import.meta.dirname, 'data', 'greeting.txt'), 'utf8');
assert.strictEqual(greeting, 'Hello from SEA VFS!');
console.log('fs access through import.meta.dirname passed');

// Static ESM import from the VFS.
assert.strictEqual(add(2, 3), 5);
assert.strictEqual(multiply(4, 5), 20);
console.log('static relative import passed');

// CommonJS interop from the VFS.
assert.strictEqual(calculator.sum(10, 20), 30);
console.log('static import of CommonJS module passed');

// Bare specifier resolution confined to the in-VFS node_modules.
assert.strictEqual(pkgName, 'test-esm-pkg');
assert.strictEqual(greet('World'), 'Hello, World!');
console.log('bare specifier import passed');

// Dynamic import from the VFS.
const dynamicMath = await import('./modules/math.mjs');
assert.strictEqual(dynamicMath.add(1, 1), 2);
console.log('dynamic import passed');

// createRequire against the VFS main URL.
const require = createRequire(import.meta.url);
const requiredCalculator = require('./modules/calculator.cjs');
assert.strictEqual(requiredCalculator.sum(3, 4), 7);
console.log('createRequire from import.meta.url passed');

// node:sea and the VFS serve the same content.
const { getAsset } = await import('node:sea');
assert.strictEqual(getAsset('data/greeting.txt', 'utf8'), greeting);
console.log('node:sea API and VFS coexistence passed');

console.log('All SEA VFS ESM tests passed!');
