'use strict';

// REPL regression test for IIFE https://github.com/nodejs/node/issues/38685

require('../common');
const util = require('util');
const assert = require('assert');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');

// Create a dummy stream that does nothing
const stream = new ArrayStream();
let output = '';

function testNormalIIFE() {
  const server = runRepl();

  stream.run(['(() => true)()']);
  assert.strictEqual(output, '> true\n> ');
  output = '';
  server.close();
  testAsyncIIEF();
}

function testAsyncIIEF() {
  const server = runRepl();
  const asyncFn = '(async() => {' +
    'await new Promise((resolve, reject) => {' +
      'resolve(true)' +
    '});' +
  '})()';

  stream.run([asyncFn]);
  // promise output twice
  output = util.inspect(output).split('\\n> ')[1];
  assert.strictEqual(output, 'Promise { <pending> }');
  output = '';
  server.close();
}

// run repl
function runRepl() {
  stream.write = function(d) {
    output += d;
  };

  return repl.start({
    input: stream,
    output: stream
  });
}

// start
testNormalIIFE();
