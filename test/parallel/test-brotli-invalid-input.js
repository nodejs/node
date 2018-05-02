// Flags: --expose-brotli
'use strict';
// test uncompressing invalid input

const common = require('../common');
const assert = require('assert');
const brotli = require('brotli');

const nonStringInputs = [
  1,
  true,
  { a: 1 },
  ['a']
];

nonStringInputs.forEach(common.mustCall((input) => {
  // brotli.decompress should not throw an error when called with bad input.
  brotli.decompress(input, function(err, buffer) {
    // brotli.decompress should pass the error to the callback.
    assert.ok(err);
  });
}, nonStringInputs.length));

const uz = new brotli.Decompress();
uz.on('error', common.mustCall());
uz.on('end', common.mustNotCall);

// this will trigger error event
uz.write('this is not valid compressed data.');
