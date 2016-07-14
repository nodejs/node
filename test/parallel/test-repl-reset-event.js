'use strict';
var common = require('../common');
common.globalCheck = false;

var assert = require('assert');
var repl = require('repl');

// Create a dummy stream that does nothing
const dummy = new common.ArrayStream();

function testReset(cb) {
  var r = repl.start({
    input: dummy,
    output: dummy,
    useGlobal: false
  });
  r.context.foo = 42;
  r.on('reset', function(context) {
    assert(!!context, 'REPL did not emit a context with reset event');
    assert.equal(context, r.context, 'REPL emitted incorrect context');
    assert.equal(context.foo, undefined, 'REPL emitted the previous context' +
                 ', and is not using global as context');
    context.foo = 42;
    cb();
  });
  r.resetContext();
}

function testResetGlobal(cb) {
  var r = repl.start({
    input: dummy,
    output: dummy,
    useGlobal: true
  });
  r.context.foo = 42;
  r.on('reset', function(context) {
    assert.equal(context.foo, 42,
                 '"foo" property is missing from REPL using global as context');
    cb();
  });
  r.resetContext();
}

var timeout = setTimeout(function() {
  common.fail('Timeout, REPL did not emit reset events');
}, 5000);

testReset(function() {
  testResetGlobal(function() {
    clearTimeout(timeout);
  });
});
