'use strict';
const common = require('../common');
const assert = require('assert');
const Script = require('vm').Script;

common.globalCheck = false;

// Run a string
let script = new Script('\'passed\';');
const result = script.runInThisContext(script);
assert.strictEqual('passed', result);

// Thrown error
script = new Script('throw new Error(\'test\');');
assert.throws(() => {
  script.runInThisContext(script);
}, /^Error: test$/);

global.hello = 5;
script = new Script('hello = 2');
script.runInThisContext(script);
assert.strictEqual(2, global.hello);


// Pass values
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (typeof baz !== "undefined") throw new Error("test fail");';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
script = new Script(global.code);
script.runInThisContext(script);
assert.strictEqual(0, global.obj.foo);
assert.strictEqual(2, global.bar);
assert.strictEqual(1, global.foo);

// Call a function
global.f = function() { global.foo = 100; };
script = new Script('f()');
script.runInThisContext(script);
assert.strictEqual(100, global.foo);
