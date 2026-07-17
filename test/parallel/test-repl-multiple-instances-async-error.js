'use strict';

// This test verifies that when multiple REPL instances exist concurrently,
// async errors are correctly routed to the REPL instance that created them.

const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const { Writable, PassThrough } = require('stream');

// Create two REPLs with separate inputs and outputs
let output1 = '';
let output2 = '';

const input1 = new PassThrough();
const input2 = new PassThrough();

const writable1 = new Writable({
  write(chunk, encoding, callback) {
    output1 += chunk.toString();
    callback();
  }
});

const writable2 = new Writable({
  write(chunk, encoding, callback) {
    output2 += chunk.toString();
    callback();
  }
});

const r1 = repl.start({
  input: input1,
  output: writable1,
  terminal: false,
  prompt: 'R1> ',
});

const r2 = repl.start({
  input: input2,
  output: writable2,
  terminal: false,
  prompt: 'R2> ',
});

// Create async error in REPL 1
input1.write('setTimeout(() => { throw new Error("error from repl1") }, 10)\n');

// Create async error in REPL 2
input2.write('setTimeout(() => { throw new Error("error from repl2") }, 20)\n');

setTimeout(common.mustCall(() => {
  r1.close();
  r2.close();

  // Verify error from REPL 1 went to REPL 1's output
  assert.match(output1, /error from repl1/,
               'REPL 1 should have received its own async error');

  // Verify error from REPL 2 went to REPL 2's output
  assert.match(output2, /error from repl2/,
               'REPL 2 should have received its own async error');

  // Verify errors did not cross over to wrong REPL
  assert.doesNotMatch(output1, /error from repl2/,
                      'REPL 1 should not have received REPL 2\'s error');
  assert.doesNotMatch(output2, /error from repl1/,
                      'REPL 2 should not have received REPL 1\'s error');
}), 100);
