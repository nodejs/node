'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const sandbox = {};
vm.createContext(sandbox);
const code = `Object.defineProperty(
               this,
               'foo',
               { get: function() {return 17} }
             );
             var desc = Object.getOwnPropertyDescriptor(this, 'foo');`;

vm.runInContext(code, sandbox);
assert.strictEqual(typeof sandbox.desc.get, 'function');
