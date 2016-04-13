'use strict';

const common = require('../common');
const assert = require('assert');

var c = 0;

process.on('exit', (code) => {
  assert.equal(c, 100);
  assert.equal(code, 0);
});

process.gracefulExit(0, 1000, common.mustCall((code, exit) => {
  c = 100;
  setTimeout(() => {
    c = 200;
  }, 2000);
}));
