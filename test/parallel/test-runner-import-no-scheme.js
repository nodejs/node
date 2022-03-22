'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

assert.throws(
  () => require('test'),
  common.expectsError({ code: 'MODULE_NOT_FOUND' }),
);

(async () => {
  await assert.rejects(
    async () => import('test'),
    common.expectsError({ code: 'ERR_MODULE_NOT_FOUND' }),
  );
})().then(common.mustCall());

assert.throws(
  () => require.resolve('test'),
  common.expectsError({ code: 'MODULE_NOT_FOUND' }),
);

// Verify that files in node_modules can be resolved.
tmpdir.refresh();

const packageRoot = path.join(tmpdir.path, 'node_modules', 'test');
const indexFile = path.join(packageRoot, 'index.js');
const cjsFile = path.join(tmpdir.path, 'cjs.js');
const esmFile = path.join(tmpdir.path, 'esm.mjs');

fs.mkdirSync(packageRoot, { recursive: true });
fs.writeFileSync(indexFile, 'module.exports = { marker: 1 };');
fs.writeFileSync(cjsFile, `
'use strict';
const common = require('../common');
const assert = require('assert');
const test = require('test');

assert.deepStrictEqual(test, { marker: 1 });
assert.strictEqual(require.resolve('test'), '${indexFile}');

(async function() {
  const dynamicImportTest = await import('test');

  assert.deepStrictEqual(dynamicImportTest.default, { marker: 1 });
})().then(common.mustCall());
`);
fs.writeFileSync(esmFile, `
import { mustCall } from '../common/index.mjs';
import assert from 'assert';
import { createRequire } from 'module';
import test from 'test';

assert.deepStrictEqual(test, { marker: 1 });

const dynamicImportTest = await import('test');
assert.deepStrictEqual(dynamicImportTest.default, { marker: 1 });

const require = createRequire(import.meta.url);
const requireTest = require('test');

assert.deepStrictEqual(requireTest, { marker: 1 });
assert.strictEqual(require.resolve('test'), '${indexFile}');
`);

let child = spawnSync(process.execPath, [cjsFile], { cwd: tmpdir.path });

assert.strictEqual(child.stdout.toString().trim(), '');
assert.strictEqual(child.stderr.toString().trim(), '');
assert.strictEqual(child.status, 0);
assert.strictEqual(child.signal, null);

child = spawnSync(process.execPath, [esmFile], { cwd: tmpdir.path });

assert.strictEqual(child.stdout.toString().trim(), '');
assert.strictEqual(child.stderr.toString().trim(), '');
assert.strictEqual(child.status, 0);
assert.strictEqual(child.signal, null);
