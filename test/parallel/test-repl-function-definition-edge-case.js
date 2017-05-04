// Reference: https://github.com/nodejs/node/pull/7624
'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');

common.globalCheck = false;

const r = initRepl();

r.input.emit('data', 'function a() { return 42; } (1)\n');
r.input.emit('data', 'a\n');
r.input.emit('data', '.exit');

const expected = '1\n[Function: a]\n';
const got = r.output.accumulator.join('');
assert.strictEqual(got, expected);

function initRepl() {
  const input = new stream();
  input.write = input.pause = input.resume = () => {};
  input.readable = true;

  const output = new stream();
  output.writable = true;
  output.accumulator = [];

  output.write = (data) => output.accumulator.push(data);

  return repl.start({
    input,
    output,
    useColors: false,
    terminal: false,
    prompt: ''
  });
}
