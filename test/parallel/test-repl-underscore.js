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
    '10',
    '10',
    'expression assignment to _ now disabled',
    '20',
    '20',
    '30',
    '20',
    '40',
    '40',
    '40'
  ]);
});

const r = repl.start({
  prompt: '',
  input: inputStream,
  output: outputStream
});

r.write('_;\n');
r.write('x = 10;\n');
r.write('_;\n');
r.write('_ = 20;\n');
r.write('_;\n');
r.write('y = 30;\n');
r.write('_;\n');
r.write('_ = 40;\n');
r.write('_;\n');
r.resetContext();
r.write('_;\n');
