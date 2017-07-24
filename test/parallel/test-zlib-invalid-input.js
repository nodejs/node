'use strict';
// test uncompressing invalid input

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const nonStringInputs = [
  1,
  true,
  { a: 1 },
  ['a']
];

// zlib.Unzip classes need to get valid data, or else they'll throw.
const unzips = [
  zlib.Unzip(),
  zlib.Gunzip(),
  zlib.Inflate(),
  zlib.InflateRaw()
];

nonStringInputs.forEach(common.mustCall((input) => {
  // zlib.gunzip should not throw an error when called with bad input.
  assert.doesNotThrow(function() {
    zlib.gunzip(input, function(err, buffer) {
      // zlib.gunzip should pass the error to the callback.
      assert.ok(err);
    });
  });
}, nonStringInputs.length));

unzips.forEach(common.mustCall((uz, i) => {
  uz.on('error', common.mustCall());
  uz.on('end', common.mustNotCall);

  // this will trigger error event
  uz.write('this is not valid compressed data.');
}, unzips.length));
