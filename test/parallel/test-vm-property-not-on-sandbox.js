'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// This, admittedly contrived, example tests an edge cases of the vm module.
//
// The GetterCallback explicitly checks the global_proxy() if a property is
// not found on the sandbox. In the following test, the explicit check
// inside the callback yields different results than deferring the
// check until after the callback. The check is deferred if the
// callback does not intercept, i.e., if args.GetReturnValue().Set() is
// not called.

// Check that the GetterCallback explicitly calls GetRealNamedProperty()
// on the global proxy if the property is not found on the sandbox.
//
// foo is not defined on the sandbox until we call CopyProperties().
// In the GetterCallback, we do not find the property on the sandbox and
// get the property from the global proxy. Since the return value is
// the sandbox, we replace it by
// the global_proxy to keep the correct identities.
//
// This test case is partially inspired by
// https://github.com/nodejs/node/issues/855
const sandbox = {console};
sandbox.document = {defaultView: sandbox};
vm.createContext(sandbox);
const code = `Object.defineProperty(
               this,
               'foo',
               { get: function() {return document.defaultView} }
             );
             var result = foo === this;`;

vm.runInContext(code, sandbox);
assert.strictEqual(sandbox.result, true);
