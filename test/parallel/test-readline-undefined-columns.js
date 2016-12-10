'use strict';

const common = require('../common');
const assert = require('assert');
const PassThrough = require('stream').PassThrough;
const readline = require('readline');

// Checks that tab completion still works
// when output column size is undefined

const iStream = new PassThrough();
const oStream = new PassThrough();

readline.createInterface({
  terminal: true,
  input: iStream,
  output: oStream,
  completer: function(line, cb) {
    cb(null, [['process.stdout', 'process.stdin', 'process.stderr'], line]);
  }
});

var output = '';

oStream.on('data', function(data) {
  output += data;
});

oStream.on('end', common.mustCall(() => {
  const expect = 'process.stdout\r\n' +
                 'process.stdin\r\n' +
                 'process.stderr';
  assert(new RegExp(expect).test(output));
}));

iStream.write('process.s\t');

assert(/process.std\b/.test(output));  // Completion works.
assert(!/stdout/.test(output));  // Completion doesn’t show all results yet.

iStream.write('\t');
oStream.end();
