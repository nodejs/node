'use strict';
// Addons: test_handle_scope, test_handle_scope_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

// Testing handle scope api calls
const testHandleScope = require(addonPath);

testHandleScope.NewScope();

assert.ok(testHandleScope.NewScopeEscape() instanceof Object);

testHandleScope.NewScopeEscapeTwice();

assert.throws(
  () => {
    testHandleScope.NewScopeWithException(() => { throw new RangeError(); });
  },
  RangeError);
