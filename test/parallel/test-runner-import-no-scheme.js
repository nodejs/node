'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const { createRequire } = require('module');

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

fs.mkdirSync(packageRoot, { recursive: true });
fs.writeFileSync(indexFile, 'module.exports = { marker: 1 };');

function test(argv) {
  const child = spawnSync(process.execPath, argv, { cwd: tmpdir.path });
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.stdout.toString().trim(), '{ marker: 1 }');
}

test(['-e', 'console.log(require("test"))']);
test(['-e', 'import("test").then(m=>console.log(m.default))']);
test(['--input-type=module', '-e', 'import test from "test";console.log(test)']);
test(['--input-type=module', '-e', 'console.log((await import("test")).default)']);

{
  const dummyFile = path.join(tmpdir.path, 'file.js');
  const require = createRequire(dummyFile);
  assert.strictEqual(require.resolve('test'), indexFile);
}
