'use strict';
const common = require('../common');
const assert = require('assert');

let mainFinished = false;

setImmediate(common.mustCall(function() {
  assert.strictEqual(mainFinished, true);
  clearImmediate(immediateB);
}));

const immediateB = setImmediate(common.mustNotCall());

for (let n = 1; n <= 5; n++) {
  const args = new Array(n).fill(0).map((_, i) => i);

  const callback = common.mustCall(function() {
    assert.deepStrictEqual(Array.from(arguments), args);
  });

  setImmediate.apply(null, [callback].concat(args));
}

mainFinished = true;
