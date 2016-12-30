'use strict';
require('../common');
const assert = require('assert');

var func = function() {};
var toStringCalled = false;
func.toString = function() {
  toStringCalled = true;
};

require('util').inspect(func);

assert.ok(!toStringCalled);
