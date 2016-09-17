'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

// The sandbox should have its own Symbol constructor.
var sandbox = {};
vm.runInNewContext('this.Symbol = Symbol', sandbox);
assert.strictEqual(typeof sandbox.Symbol, 'function');
assert.notStrictEqual(sandbox.Symbol, Symbol);

// Unless we copy the Symbol constructor explicitly, of course.
sandbox = { Symbol: Symbol };
vm.runInNewContext('this.Symbol = Symbol', sandbox);
assert.strictEqual(typeof sandbox.Symbol, 'function');
assert.strictEqual(sandbox.Symbol, Symbol);
