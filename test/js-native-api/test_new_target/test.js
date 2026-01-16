'use strict';
// Addons: test_new_target, test_new_target_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const binding = require(addonPath);

class Class extends binding.BaseClass {
  constructor() {
    super();
    this.method();
  }
  method() {
    this.ok = true;
  }
}

assert.ok(new Class() instanceof binding.BaseClass);
assert.ok(new Class().ok);
assert.ok(binding.OrdinaryFunction());
assert.ok(
  new binding.Constructor(binding.Constructor) instanceof binding.Constructor);
