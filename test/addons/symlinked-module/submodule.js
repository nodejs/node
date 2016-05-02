'use strict';
require('common');
const path = require('path');
const assert = require('assert');

module.exports.test = function(bindingDir) {
  var mod = require(path.join(bindingDir, 'binding.node'));
  assert(mod != null);
  assert(mod.hello() == 'world');
}
