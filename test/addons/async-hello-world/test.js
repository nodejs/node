'use strict';
const common = require('../../common');
var assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);

binding(5, common.mustCall(function(err, val) {
  assert.equal(null, err);
  assert.equal(10, val);
  process.nextTick(common.mustCall(function() {}));
}));
