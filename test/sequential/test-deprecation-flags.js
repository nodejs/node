'use strict';
const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const depmod = require.resolve('../fixtures/deprecated.js');
const node = process.execPath;

const depUserland =
    require.resolve(common.fixturesDir + '/deprecated-userland-function.js');

const normal = [depmod];
const noDep = ['--no-deprecation', depmod];
const traceDep = ['--trace-deprecation', depmod];

execFile(node, normal, function(er, stdout, stderr) {
  console.error('normal: show deprecation warning');
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert.equal(stderr, '(node) util.p is deprecated. Use console.error ' +
                       'instead.\n\'This is deprecated\'\n');
  console.log('normal ok');
});

execFile(node, noDep, function(er, stdout, stderr) {
  console.error('--no-deprecation: silence deprecations');
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert.equal(stderr, '\'This is deprecated\'\n');
  console.log('silent ok');
});

execFile(node, traceDep, function(er, stdout, stderr) {
  console.error('--trace-deprecation: show stack');
  assert.equal(er, null);
  assert.equal(stdout, '');
  const stack = stderr.trim().split('\n');
  // just check the top and bottom.
  assert.equal(stack[0],
               'Trace: util.p is deprecated. Use console.error instead.');
  assert.equal(stack.pop(), '\'This is deprecated\'');
  console.log('trace ok');
});

execFile(node, [depUserland], function(er, stdout, stderr) {
  console.error('normal: testing deprecated userland function');
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert.equal(0, stderr.indexOf('deprecatedFunction is deprecated.'));
  console.error('normal: ok');
});
