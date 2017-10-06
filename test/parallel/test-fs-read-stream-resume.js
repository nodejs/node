'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

const fs = require('fs');

const file = fixtures.path('x.txt');
let data = '';
let first = true;

const stream = fs.createReadStream(file);
stream.setEncoding('utf8');
stream.on('data', function(chunk) {
  data += chunk;
  if (first) {
    first = false;
    stream.resume();
  }
});

process.nextTick(function() {
  stream.pause();
  setTimeout(function() {
    stream.resume();
  }, 100);
});

process.on('exit', function() {
  assert.strictEqual(data, 'xyz\n');
});
