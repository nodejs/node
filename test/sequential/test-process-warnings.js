// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const warnmod = require.resolve('../fixtures/warnings.js');
const node = process.execPath;

const normal = ['--expose-internals', warnmod];
const noWarn = ['--expose-internals', '--no-warnings', warnmod];
const traceWarn = ['--expose-internals', '--trace-warnings', warnmod];

execFile(node, normal, function(er, stdout, stderr) {
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(/SecurityWarning: a security warning/.test(stderr));
  assert(/BadPracticeWarning: a bad practice warning/.test(stderr));
  assert(/DeprecationWarning: a deprecation warning/.test(stderr));
});

execFile(node, noWarn, function(er, stdout, stderr) {
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(!/SecurityWarning: a security warning/.test(stderr));
  assert(!/BadPracticeWarning: a bad practice warning/.test(stderr));
  assert(!/DeprecationWarning: a deprecation warning/.test(stderr));
});

execFile(node, traceWarn, function(er, stdout, stderr) {
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(/SecurityWarning: a security warning/.test(stderr));
  assert(/BadPracticeWarning: a bad practice warning/.test(stderr));
  assert(/DeprecationWarning: a deprecation warning/.test(stderr));
  assert(/at Object\.\<anonymous\>\s\(.+warnings.js:3:25\)/.test(stderr));
  assert(/at Object\.\<anonymous\>\s\(.+warnings.js:8:28\)/.test(stderr));
  // DeprecationWarning will only show a trace if --trace-deprecation is on
});
