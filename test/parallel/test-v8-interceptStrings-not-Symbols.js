'use strict';
require('../common');

const assert = require('assert');

// Test that the v8 named property handler intercepts callbacks
// when properties are defined as Strings and NOT for Symbols.
//
// With the kOnlyInterceptStrings flag, manipulating properties via
// Strings is intercepted by the callbacks, while Symbols adopt
// the default global behaviour.
// Removing the kOnlyInterceptStrings flag, adds intercepting to Symbols,
// which causes Type Error at process.env[symbol]=42 due to process.env being
// strongly typed for Strings
// (node::Utf8Value key(info.GetIsolate(), property);).


const symbol = Symbol('sym');

// check if its undefined
assert.strictEqual(process.env[symbol], undefined);

// set a value using a Symbol
process.env[symbol] = 42;

// set a value using a String (call to EnvSetter, node.cc)
process.env['s'] = 42;

//check the values after substitutions
assert.strictEqual(42, process.env[symbol]);
assert.strictEqual('42', process.env['s']);

delete process.env[symbol];
assert.strictEqual(undefined, process.env[symbol]);
