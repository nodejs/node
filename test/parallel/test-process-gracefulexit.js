'use strict';

const common = require('../common');
const assert = require('assert');

var c = 0;
var m = false;

process.on('exit', (code) => {
  assert.equal(c, 3);
  assert(m);
  assert.equal(code, 0);
});

process.gracefulExit(1, common.mustCall((code, exit) => {
  assert.equal(code, 1);
  assert.equal(c, 0);
  c++; // 1
  process.nextTick(common.mustCall(() => {
    assert.equal(c, 1);
    c++; // 2
    setImmediate(common.mustCall(() => {
      assert.equal(c, 2);
      c++; // 3
      exit(0);
    }));
  }));
}));

m = true;
