'use strict';

require('../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');

const inputStream = new stream.PassThrough();
const outputStream = new stream.PassThrough();
outputStream.accum = '';

outputStream.on('data', (data) => {
  outputStream.accum += data;
});

const r = repl.start({
  input: inputStream,
  output: outputStream,
  useColors: false,
  terminal: false,
  prompt: ''
});

r.write('const str = `tacos \\\nfoobar`');
assert.strictEqual(r.output.accum, '... ');
