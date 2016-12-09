'use strict';

const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const warnmod = require.resolve(common.fixturesDir + '/warnings.js');
const node = process.execPath;

const normal = [warnmod];
const noWarn = ['--no-warnings', warnmod];
const traceWarn = ['--trace-warnings', warnmod];

execFile(node, normal, function(er, stdout, stderr) {
  // Show Process Warnings
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(/^\(.+\)\sWarning: a bad practice warning/.test(stderr));
});

execFile(node, noWarn, function(er, stdout, stderr) {
  // Hide Process Warnings
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(!/^\(.+\)\sWarning: a bad practice warning/.test(stderr));
});

execFile(node, traceWarn, function(er, stdout, stderr) {
  // Show Warning Trace
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(/^\(.+\)\sWarning: a bad practice warning/.test(stderr));
  assert(/at Object\.<anonymous>\s\(.+warnings.js:3:9\)/.test(stderr));
});
