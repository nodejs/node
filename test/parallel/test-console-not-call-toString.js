'use strict';
require('../common');
const assert = require('assert');

const func = function() {};
let toStringCalled = false;
func.toString = function() {
  toStringCalled = true;
};

require('util').inspect(func);

assert.ok(!toStringCalled);
