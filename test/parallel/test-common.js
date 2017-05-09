'use strict';
const common = require('../common');
const assert = require('assert');

const {join} = require('path');
const {execFile} = require('child_process');

common.globalCheck = false;
global.gc = 42;  // Not a valid global unless --expose_gc is set.
assert.deepStrictEqual(common.leakedGlobals(), ['gc']);

assert.throws(function() {
  common.mustCall(common.noop, 'foo');
}, /^TypeError: Invalid exact value: foo$/);

assert.throws(function() {
  common.mustCall(common.noop, /foo/);
}, /^TypeError: Invalid exact value: \/foo\/$/);

const fnOnce = common.mustCall(() => {});
fnOnce();
const fnTwice = common.mustCall(() => {}, 2);
fnTwice();
fnTwice();
const fnAtLeast1Called1 = common.mustCallAtLeast(() => {}, 1);
fnAtLeast1Called1();
const fnAtLeast1Called2 = common.mustCallAtLeast(() => {}, 1);
fnAtLeast1Called2();
fnAtLeast1Called2();
const fnAtLeast2Called2 = common.mustCallAtLeast(() => {}, 2);
fnAtLeast2Called2();
fnAtLeast2Called2();
const fnAtLeast2Called3 = common.mustCallAtLeast(() => {}, 2);
fnAtLeast2Called3();
fnAtLeast2Called3();
fnAtLeast2Called3();

const failFixtures = [
  [
    join(common.fixturesDir, 'failmustcall1.js'),
    'Mismatched <anonymous> function calls. Expected exactly 2, actual 1.'
  ], [
    join(common.fixturesDir, 'failmustcall2.js'),
    'Mismatched <anonymous> function calls. Expected at least 2, actual 1.'
  ]
];
for (const p of failFixtures) {
  const [file, expected] = p;
  execFile(process.argv[0], [file], common.mustCall((ex, stdout, stderr) => {
    assert.ok(ex);
    assert.strictEqual(stderr, '');
    const firstLine = stdout.split('\n').shift();
    assert.strictEqual(firstLine, expected);
  }));
}
