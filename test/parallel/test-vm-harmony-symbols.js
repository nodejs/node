'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

// The sandbox should have its own Symbol constructor.
var sandbox = {};
vm.runInNewContext('this.Symbol = Symbol', sandbox);
assert(typeof sandbox.Symbol === 'function');
assert(sandbox.Symbol !== Symbol);

// Unless we copy the Symbol constructor explicitly, of course.
sandbox = { Symbol: Symbol };
vm.runInNewContext('this.Symbol = Symbol', sandbox);
assert(typeof sandbox.Symbol === 'function');
assert(sandbox.Symbol === Symbol);
