'use strict';
// test uncompressing invalid input

require('../common');
const assert = require('assert');
const zlib = require('zlib');

var nonStringInputs = [1, true, {a: 1}, ['a']];

console.error('Doing the non-strings');
nonStringInputs.forEach(function(input) {
  // zlib.gunzip should not throw an error when called with bad input.
  assert.doesNotThrow(function() {
    zlib.gunzip(input, function(err, buffer) {
      // zlib.gunzip should pass the error to the callback.
      assert.ok(err);
    });
  });
});

console.error('Doing the unzips');
// zlib.Unzip classes need to get valid data, or else they'll throw.
var unzips = [ zlib.Unzip(),
               zlib.Gunzip(),
               zlib.Inflate(),
               zlib.InflateRaw() ];
var hadError = [];
unzips.forEach(function(uz, i) {
  console.error('Error for ' + uz.constructor.name);
  uz.on('error', function(er) {
    console.error('Error event', er);
    hadError[i] = true;
  });

  uz.on('end', function(er) {
    throw new Error('end event should not be emitted ' + uz.constructor.name);
  });

  // this will trigger error event
  uz.write('this is not valid compressed data.');
});

process.on('exit', function() {
  assert.deepStrictEqual(hadError, [true, true, true, true], 'expect 4 errors');
});
