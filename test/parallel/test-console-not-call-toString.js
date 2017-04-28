'use strict';
require('../common');
const assert = require('assert');

function func() {}
let toStringCalled = false;
func.toString = function() {
  toStringCalled = true;
};

require('util').inspect(func);

assert.ok(!toStringCalled);
