'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const { createRequire } = require('module');

for (const name in ['test', 'test/reporters']) {
  assert.throws(
    () => require(name),
    common.expectsError({ code: 'MODULE_NOT_FOUND' }),
  );

  (async () => {
    await assert.rejects(
      async () => import(name),
      common.expectsError({ code: 'ERR_MODULE_NOT_FOUND' }),
    );
  })().then(common.mustCall());

  assert.throws(
    () => require.resolve(name),
    common.expectsError({ code: 'MODULE_NOT_FOUND' }),
  );
}

// Verify that files in node_modules can be resolved.
tmpdir.refresh();

const packageRoot = tmpdir.resolve('node_modules', 'test');
const reportersDir = tmpdir.resolve('node_modules', 'test', 'reporters');
const indexFile = path.join(packageRoot, 'index.js');
const reportersIndexFile = path.join(reportersDir, 'index.js');

fs.mkdirSync(reportersDir, { recursive: true });
fs.writeFileSync(indexFile, 'module.exports = { marker: 1 };');
fs.writeFileSync(reportersIndexFile, 'module.exports = { marker: 1 };');

function test(argv, expectedToFail = false) {
  const child = spawnSync(process.execPath, argv, { cwd: tmpdir.path });
  if (expectedToFail) {
    assert.strictEqual(child.status, 1);
    assert.strictEqual(child.stdout.toString().trim(), '');
  } else {
    assert.strictEqual(child.status, 0);
    assert.strictEqual(child.stdout.toString().trim(), '{ marker: 1 }');
  }
}

test(['-e', 'console.log(require("test"))']);
test(['-e', 'console.log(require("test/reporters"))']);
test(['-e', 'import("test").then(m=>console.log(m.default))']);
test(['-e', 'import("test/reporters").then(m=>console.log(m.default))'], true);
test(['--input-type=module', '-e', 'import test from "test";console.log(test)']);
test(['--input-type=module', '-e', 'import test from "test/reporters";console.log(test)'], true);
test(['--input-type=module', '-e', 'console.log((await import("test")).default)']);
test(['--input-type=module', '-e', 'console.log((await import("test/reporters")).default)'], true);

{
  const dummyFile = tmpdir.resolve('file.js');
  const require = createRequire(dummyFile);
  assert.strictEqual(require.resolve('test'), indexFile);
  assert.strictEqual(require.resolve('test/reporters'), reportersIndexFile);
}
