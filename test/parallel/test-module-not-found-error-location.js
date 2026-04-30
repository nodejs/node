'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('assert');
const { spawnSync } = require('child_process');
const { writeFileSync } = require('fs');

tmpdir.refresh();

const entry = tmpdir.resolve('entry.cjs');
writeFileSync(entry, [
  'const ok = require("node:fs");',
  'const missing = require("this-package-does-not-exist");',
  'console.log(ok, missing);',
].join('\n'));

const { status, signal, stdout, stderr } = spawnSync(process.execPath, [entry], {
  encoding: 'utf8',
});

assert.strictEqual(status, 1);
assert.strictEqual(signal, null);
assert.strictEqual(stdout, '');
assert.ok(stderr.includes(`${entry}:2`));
assert.match(stderr, /const missing = require\("this-package-does-not-exist"\);/);
assert.match(stderr, / {25}\^{27}/);
assert.match(stderr, /MODULE_NOT_FOUND/);
assert.match(stderr, /Require stack:/);

const longLineEntry = tmpdir.resolve('long-line-entry.cjs');
writeFileSync(longLineEntry, [
  `const prefix = "${'x'.repeat(140)}"; require("this-package-does-not-exist");`,
].join('\n'));

const longLineResult = spawnSync(process.execPath, [longLineEntry], {
  encoding: 'utf8',
});

assert.strictEqual(longLineResult.status, 1);
assert.strictEqual(longLineResult.signal, null);
assert.strictEqual(longLineResult.stdout, '');
assert.ok(longLineResult.stderr.includes(`${longLineEntry}:1`));
assert.match(longLineResult.stderr, /\.\.\.x+"; require\("this-package-does-not-exist"\);/);
assert.match(longLineResult.stderr, / {43}\^{27}/);
assert.match(longLineResult.stderr, /MODULE_NOT_FOUND/);

const variableEntry = tmpdir.resolve('variable-entry.cjs');
writeFileSync(variableEntry, [
  'const pkg = "this-package-does-not-exist";',
  'require(pkg);',
].join('\n'));

const variableResult = spawnSync(process.execPath, [variableEntry], {
  encoding: 'utf8',
});

assert.strictEqual(variableResult.status, 1);
assert.strictEqual(variableResult.signal, null);
assert.strictEqual(variableResult.stdout, '');
assert.ok(variableResult.stderr.includes(`${variableEntry}:2`));
assert.match(variableResult.stderr, /require\(pkg\);/);
assert.match(variableResult.stderr, /^\^{27}$/m);
assert.match(variableResult.stderr, /MODULE_NOT_FOUND/);
