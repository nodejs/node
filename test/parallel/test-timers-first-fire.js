'use strict';
var common = require('../common');
var assert = require('assert');

var TIMEOUT = 50;
var last = process.hrtime();
setTimeout(function() {
  var hr = process.hrtime(last);
  var ms = (hr[0] * 1e3) + (hr[1] / 1e6);
  var delta = ms - TIMEOUT;
  console.log('timer fired in', delta);
  assert.ok(delta > -0.5, 'Timer fired early');
}, TIMEOUT);
