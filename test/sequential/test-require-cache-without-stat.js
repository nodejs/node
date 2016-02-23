'use strict';
// We've experienced a regression where the module loader stats a bunch of
// directories on require() even if it's been called before. The require()
// should caching the request.
var common = require('../common');
var fs = require('fs');
var assert = require('assert');

var counter = 0;

// Switch out the two stat implementations so that they increase a counter
// each time they are called.

var _statSync = fs.statSync;
var _stat = fs.stat;

fs.statSync = function() {
  counter++;
  return _statSync.apply(this, arguments);
};

fs.stat = function() {
  counter++;
  return _stat.apply(this, arguments);
};

// Load the module 'a' and 'http' once. It should become cached.
require(common.fixturesDir + '/a');
require('../fixtures/a.js');
require('./../fixtures/a.js');
require('http');

console.log('counterBefore = %d', counter);
var counterBefore = counter;

// Now load the module a bunch of times with equivalent paths.
// stat should not be called.
for (let i = 0; i < 100; i++) {
  require(common.fixturesDir + '/a');
  require('../fixtures/a.js');
  require('./../fixtures/a.js');
}

// Do the same with a built-in module
for (let i = 0; i < 100; i++) {
  require('http');
}

console.log('counterAfter = %d', counter);
var counterAfter = counter;

assert.equal(counterBefore, counterAfter);
