'use strict';

const assert = require('assert');
var uvRunOnce = require('./build/Release/binding');
setTimeout(function () {
  var res;
  setTimeout(function () {
    res = true;
  }, 2);
  while (!res && uvRunOnce.run()) {
  }
  assert.equal(res, true);
}, 2);