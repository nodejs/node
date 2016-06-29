'use strict';
var common = require('../common');
var path = require('path');
var assert = require('assert');

var a;
setTimeout(function() {
  a = require(path.join(common.fixturesDir, 'a'));
}, 50);

process.on('exit', function() {
  assert.equal(true, 'A' in a);
  assert.equal('A', a.A());
  assert.equal('D', a.D());
});
