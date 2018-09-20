'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');

assert.throws(function() {
  fs.write(null, Buffer.allocUnsafe(1), 0, 1, common.mustNotCall());
}, /TypeError/);

assert.throws(function() {
  fs.write(null, '1', 0, 1, common.mustNotCall());
}, /TypeError/);
