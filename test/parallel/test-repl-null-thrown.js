'use strict';
require('../common');
const repl = require('repl');
const assert = require('assert');
const Stream = require('stream');

const output = new Stream();
let text = '';
output.write = output.pause = output.resume = function(buf) {
  text += buf.toString();
};

const replserver = repl.start({
  output: output,
  input: process.stdin
});

replserver.emit('line', 'process.nextTick(() => { throw null; })');
replserver.emit('line', '.exit');

setTimeout(() => {
  console.log(text);
  assert(text.includes('Uncaught null'));
}, 0);
