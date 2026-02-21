'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);

class Class extends binding.Class {
  constructor() {
    super();
    this.method();
  }
  method() {
    this.ok = true;
  }
}

assert.ok(new Class() instanceof binding.Class);
assert.ok(new Class().ok);
