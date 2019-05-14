'use strict';

const common = require('../../common');
const assert = require('assert');
const vm = require('vm');
const binding = require(`./build/${common.buildType}/binding`);
const makeCallback = binding.makeCallback;

function myMultiArgFunc(arg1, arg2, arg3) {
  assert.strictEqual(arg1, 1);
  assert.strictEqual(arg2, 2);
  assert.strictEqual(arg3, 3);
  return 42;
}

assert.strictEqual(makeCallback(process, common.mustCall(function() {
  assert.strictEqual(arguments.length, 0);
  assert.strictEqual(this, process);
  return 42;
})), 42);

assert.strictEqual(makeCallback(process, common.mustCall(function(x) {
  assert.strictEqual(arguments.length, 1);
  assert.strictEqual(this, process);
  assert.strictEqual(x, 1337);
  return 42;
}), 1337), 42);

assert.strictEqual(makeCallback(this,
                                common.mustCall(myMultiArgFunc), 1, 2, 3), 42);

// TODO(node-api): napi_make_callback needs to support
// strings passed for the func argument
/*
const recv = {
  one: common.mustCall(function() {
    assert.strictEqual(0, arguments.length);
    assert.strictEqual(this, recv);
    return 42;
  }),
  two: common.mustCall(function(x) {
    assert.strictEqual(1, arguments.length);
    assert.strictEqual(this, recv);
    assert.strictEqual(x, 1337);
    return 42;
  }),
};

assert.strictEqual(makeCallback(recv, 'one'), 42);
assert.strictEqual(makeCallback(recv, 'two', 1337), 42);

// Check that callbacks on a receiver from a different context works.
const foreignObject = vm.runInNewContext('({ fortytwo() { return 42; } })');
assert.strictEqual(makeCallback(foreignObject, 'fortytwo'), 42);
*/

// Check that the callback is made in the context of the receiver.
const target = vm.runInNewContext(`
    (function($Object) {
      if (Object === $Object)
        throw new Error('bad');
      return Object;
    })
`);
assert.notStrictEqual(makeCallback(process, target, Object), Object);

// Runs in inner context.
const forward = vm.runInNewContext(`
    (function(forward) {
      return forward(Object);
    })
`);

// Runs in outer context.
function endpoint($Object) {
  if (Object === $Object)
    throw new Error('bad');
  return Object;
}

assert.strictEqual(makeCallback(process, forward, endpoint), Object);
