/* eslint-disable no-debugger */
'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const spawn = require('child_process').spawn;

assert.throws(function() {
  vm.runInDebugContext('*');
}, /SyntaxError/);

assert.throws(function() {
  vm.runInDebugContext({ toString: common.fail });
}, /AssertionError/);

assert.throws(function() {
  vm.runInDebugContext('throw URIError("BAM")');
}, /URIError/);

assert.throws(function() {
  vm.runInDebugContext('(function(f) { f(f) })(function(f) { f(f) })');
}, /RangeError/);

assert.strictEqual(typeof vm.runInDebugContext('this'), 'object');
assert.strictEqual(typeof vm.runInDebugContext('Debug'), 'object');

assert.strictEqual(vm.runInDebugContext(), undefined);
assert.strictEqual(vm.runInDebugContext(0), 0);
assert.strictEqual(vm.runInDebugContext(null), null);
assert.strictEqual(vm.runInDebugContext(undefined), undefined);

// See https://github.com/nodejs/node/issues/1190, accessing named interceptors
// and accessors inside a debug event listener should not crash.
{
  const Debug = vm.runInDebugContext('Debug');
  let breaks = 0;

  function ondebugevent(evt, exc) {
    if (evt !== Debug.DebugEvent.Break) return;
    exc.frame(0).evaluate('process.env').properties();  // Named interceptor.
    exc.frame(0).evaluate('process.title').getTruncatedValue();  // Accessor.
    breaks += 1;
  }

  function breakpoint() {
    debugger;
  }

  assert.strictEqual(breaks, 0);
  Debug.setListener(ondebugevent);
  assert.strictEqual(breaks, 0);
  breakpoint();
  assert.strictEqual(breaks, 1);
}

// Can set listeners and breakpoints on a single line file
{
  const Debug = vm.runInDebugContext('Debug');
  const fn = require(common.fixturesDir + '/exports-function-with-param');
  let called = false;

  Debug.setListener(function(event, state, data) {
    if (data.constructor.name === 'BreakEvent') {
      called = true;
    }
  });

  Debug.setBreakPoint(fn);
  fn('foo');
  assert.strictEqual(Debug.showBreakPoints(fn), '(arg) { [B0]return arg; }');
  assert.strictEqual(called, true);
}

// See https://github.com/nodejs/node/issues/1190, fatal errors should not
// crash the process.
const script = common.fixturesDir + '/vm-run-in-debug-context.js';
let proc = spawn(process.execPath, [script]);
const data = [];
proc.stdout.on('data', common.mustNotCall());
proc.stderr.on('data', data.push.bind(data));
proc.stderr.once('end', common.mustCall(function() {
  const haystack = Buffer.concat(data).toString('utf8');
  assert(/SyntaxError: Unexpected token \*/.test(haystack));
}));
proc.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 1);
  assert.strictEqual(signalCode, null);
}));

proc = spawn(process.execPath, [script, 'handle-fatal-exception']);
proc.stdout.on('data', common.mustNotCall());
proc.stderr.on('data', common.mustNotCall());
proc.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 42);
  assert.strictEqual(signalCode, null);
}));
