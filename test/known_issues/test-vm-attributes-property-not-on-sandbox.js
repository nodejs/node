'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// The QueryCallback explicitly calls GetRealNamedPropertyAttributes
// on the global proxy if the property is not found on the sandbox.
//
// foo is not defined on the sandbox until we call CopyProperties().
// In the QueryCallback, we do not find the property on the sandbox
// and look up its PropertyAttributes on the global_proxy().
// PropertyAttributes are always flattened to a value
// descriptor.
const sandbox = {};
vm.createContext(sandbox);
const code = `Object.defineProperty(
               this,
               'foo',
               { get: function() {return 17} }
             );
             var desc = Object.getOwnPropertyDescriptor(this, 'foo');`;

vm.runInContext(code, sandbox);
// The descriptor is flattened. We wrongly have typeof desc.value = 'number'.
assert.strictEqual(typeof sandbox.desc.get, 'function');
