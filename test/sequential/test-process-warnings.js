'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const execFile = require('child_process').execFile;
const warnmod = require.resolve(fixtures.path('warnings.js'));
const node = process.execPath;

const normal = [warnmod];
const noWarn = ['--no-warnings', warnmod];
const traceWarn = ['--trace-warnings', warnmod];

const warningMessage = /^\(.+\)\sWarning: a bad practice warning/;

execFile(node, normal, function(er, stdout, stderr) {
  // Show Process Warnings
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(warningMessage.test(stderr));
});

execFile(node, noWarn, function(er, stdout, stderr) {
  // Hide Process Warnings
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(!warningMessage.test(stderr));
});

execFile(node, traceWarn, function(er, stdout, stderr) {
  // Show Warning Trace
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(warningMessage.test(stderr));
  assert(/at Object\.<anonymous>\s\(.+warnings\.js:3:9\)/.test(stderr));
});
