// Regression test for https://github.com/nodejs/node/issues/62043
'use strict';

const common = require('../common');
if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
}

const fs = require('fs');
const { createRequire } = require('module');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const TARGET = 260; // Shortest length that used to trigger the bug
const fixedLen = tmpdir.path.length + 2 + 'package.json'.length;
const dirNameLen = Math.max(TARGET - fixedLen, 1);

const dir = path.join(tmpdir.path, 'a'.repeat(dirNameLen));
const depDir = path.join(dir, 'node_modules', 'dep');
const packageJsonPath = path.join(dir, 'package.json');

fs.mkdirSync(depDir, { recursive: true });
fs.writeFileSync(
  packageJsonPath,
  JSON.stringify({ imports: { '#foo': './foo.mjs' } }),
);
fs.writeFileSync(path.join(dir, 'foo.mjs'), 'export default 1;\n');
fs.writeFileSync(
  path.join(depDir, 'package.json'),
  JSON.stringify({ name: 'dep', exports: { '.': './index.mjs' } }),
);
fs.writeFileSync(path.join(depDir, 'index.mjs'), 'export default 1;\n');

const req = createRequire(path.join(dir, '_.mjs'));

// Both resolves should succeed without throwing
req.resolve('dep');
req.resolve('#foo');
