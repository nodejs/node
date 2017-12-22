// Reference: https://github.com/nodejs/node/pull/7624
'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');

common.globalCheck = false;

const input = new stream();
input.write = input.pause = input.resume = () => {};
input.readable = true;

const output = new stream();
output.writable = true;
output.accumulator = [];

output.write = (data) => output.accumulator.push(data);

const replserver = repl.start({
  input,
  output,
  useColors: false,
  terminal: false,
  prompt: ''
});
const callbacks = [];
const $eval = replserver.eval;
replserver.eval = function(code, context, file, cb) {
  const expected = callbacks.shift();
  return $eval.call(this, code, context, file, (...args) => {
    try {
      expected(...args);
    } catch (e) {
      console.error(e);
      process.exit(1);
    }
    cb(...args);
  });
};

callbacks.push(common.mustCall((err, result) => {
  assert.ifError(err);
  assert.strictEqual(result, 1);
}));
replserver.input.emit('data', 'function a() { return 42; } (1)\n');
callbacks.push(common.mustCall((err, result) => {
  assert.ifError(err);
  assert.strictEqual(typeof result, 'function');
  assert.strictEqual(result.toString(), 'function a() { return 42; }');
}));
replserver.input.emit('data', 'a\n');
replserver.input.emit('data', '.exit');
