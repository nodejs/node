'use strict';
const common = require('../common');
const assert = require('assert');

let mainFinished = false;

setImmediate(common.mustCall(function() {
  assert.strictEqual(mainFinished, true);
  clearImmediate(immediateB);
}));

let immediateB = setImmediate(function() {
  common.fail('this immediate should not run');
});

setImmediate(common.mustCall((...args) => {
  assert.deepStrictEqual(args, [1, 2, 3]);
}), 1, 2, 3);

setImmediate(common.mustCall((...args) => {
  assert.deepStrictEqual(args, [1, 2, 3, 4, 5]);
}), 1, 2, 3, 4, 5);

mainFinished = true;
