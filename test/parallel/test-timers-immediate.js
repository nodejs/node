'use strict';
const common = require('../common');
const assert = require('assert');

let immediateC;
let immediateD;

let mainFinished = false;

setImmediate(common.mustCall(function() {
  assert.strictEqual(mainFinished, true);
  clearImmediate(immediateB);
}));

const immediateB = setImmediate(common.mustNotCall());

setImmediate(function(x, y, z) {
  immediateC = [x, y, z];
}, 1, 2, 3);

setImmediate(function(x, y, z, a, b) {
  immediateD = [x, y, z, a, b];
}, 1, 2, 3, 4, 5);

process.on('exit', function() {
  assert.deepStrictEqual(immediateC, [1, 2, 3], 'immediateC args should match');
  assert.deepStrictEqual(immediateD, [1, 2, 3, 4, 5], '5 args should match');
});

mainFinished = true;
