'use strict';

require('../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');
const inputStream = new stream.PassThrough();
const outputStream = new stream.PassThrough();
let output = '';

outputStream.on('data', (data) => {
  output += data;
});

process.on('exit', () => {
  const lines = output.trim().split('\n');

  assert.deepStrictEqual(lines, [
    'undefined',
    'assignment to _ is reserved by the REPL',
    '42',
    '42',
    '85',
    '85',
    'undefined'
  ]);
});

const r = repl.start({
  prompt: '',
  input: inputStream,
  output: outputStream
});

r.write('_;\n');
r.write('_ = 42;\n');
r.write('_;\n');
r.write('foo = 85;\n');
r.write('_;\n');
r.resetContext();
r.write('_;\n');
