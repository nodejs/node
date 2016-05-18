'use strict';

require('../common');
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

oStream.on('end', function() {
  const expect = 'process.stdout\r\n' +
                 'process.stdin\r\n' +
                 'process.stderr';
  assert(new RegExp(expect).test(output));
});

iStream.write('process.std\t');
oStream.end();
