'use strict';
var common = require('../common');
var assert = require('assert');

var zero = [];
var one  = [ new Buffer('asdf') ];
var long = [];
for (var i = 0; i < 10; i++) long.push(new Buffer('asdf'));

var flatZero = Buffer.concat(zero);
var flatOne = Buffer.concat(one);
var flatLong = Buffer.concat(long);
var flatLongLen = Buffer.concat(long, 40);

assert(flatZero.length === 0);
assert(flatOne.toString() === 'asdf');
// A special case where concat used to return the first item,
// if the length is one. This check is to make sure that we don't do that.
assert(flatOne !== one[0]);
assert(flatLong.toString() === (new Array(10 + 1).join('asdf')));
assert(flatLongLen.toString() === (new Array(10 + 1).join('asdf')));

assert.throws(function() {
  Buffer.concat([42]);
}, TypeError);

console.log('ok');
