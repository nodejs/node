'use strict';
const common = require('../common');
common.globalCheck = false;

const assert = require('assert');
const repl = require('repl');
const util = require('util');

// Create a dummy stream that does nothing
const dummy = new common.ArrayStream();

function testReset(cb) {
  const r = repl.start({
    input: dummy,
    output: dummy,
    useGlobal: false
  });
  r.context.foo = 42;
  r.on('reset', common.mustCall(function(context) {
    assert(!!context, 'REPL did not emit a context with reset event');
    assert.strictEqual(context, r.context, 'REPL emitted incorrect context. ' +
    `context is ${util.inspect(context)}, expected ${util.inspect(r.context)}`);
    assert.strictEqual(
      context.foo,
      undefined,
      'REPL emitted the previous context and is not using global as context. ' +
      `context.foo is ${context.foo}, expected undefined.`
    );
    context.foo = 42;
    cb();
  }));
  r.resetContext();
}

function testResetGlobal() {
  const r = repl.start({
    input: dummy,
    output: dummy,
    useGlobal: true
  });
  r.context.foo = 42;
  r.on('reset', common.mustCall(function(context) {
    assert.strictEqual(
      context.foo,
      42,
      '"foo" property is different from REPL using global as context. ' +
      `context.foo is ${context.foo}, expected 42.`
    );
  }));
  r.resetContext();
}

testReset(common.mustCall(testResetGlobal));
