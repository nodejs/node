'use strict';
var common = require('../common');
var assert = require('assert');

var a;
setTimeout(function() {
  a = require('../fixtures/a');
}, 50);

process.on('exit', function() {
  assert.equal(true, 'A' in a);
  assert.equal('A', a.A());
  assert.equal('D', a.D());
});
