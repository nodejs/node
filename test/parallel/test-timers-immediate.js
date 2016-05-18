'use strict';
require('../common');
var assert = require('assert');

let immediateA = false;
let immediateB = false;
let immediateC = [];

setImmediate(function() {
  try {
    immediateA = process.hrtime(before);
  } catch (e) {
    console.log('failed to get hrtime with offset');
  }
  clearImmediate(immediateB);
});

const before = process.hrtime();

immediateB = setImmediate(function() {
  immediateB = true;
});

setImmediate(function(x, y, z) {
  immediateC = [x, y, z];
}, 1, 2, 3);

process.on('exit', function() {
  assert.ok(immediateA, 'Immediate should happen after normal execution');
  assert.notStrictEqual(immediateB, true, 'immediateB should not fire');
  assert.deepStrictEqual(immediateC, [1, 2, 3], 'immediateC args should match');
});
