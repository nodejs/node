'use strict';
const common = require('../common');
const assert = require('assert');

const fs = require('fs');
const path = require('path');

const file = path.join(common.fixturesDir, 'x.txt');
let data = '';
let first = true;

var stream = fs.createReadStream(file);
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
