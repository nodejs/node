'use strict';

// This test verifies that when multiple REPL instances exist concurrently,
// async errors are correctly routed to the REPL instance that created them.

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('assert');

// Create two REPLs with separate inputs and outputs
const {
  replServer: r1,
  input: input1,
  output: output1,
} = startNewREPLServer({ terminal: false, prompt: 'R1> ' });

const {
  replServer: r2,
  input: input2,
  output: output2,
} = startNewREPLServer({ terminal: false, prompt: 'R2> ' });

// Create async error in REPL 1
input1.emit('data', 'setTimeout(() => { throw new Error("error from repl1") }, 10)\n');

// Create async error in REPL 2
input2.emit('data', 'setTimeout(() => { throw new Error("error from repl2") }, 20)\n');

setTimeout(common.mustCall(() => {
  r1.close();
  r2.close();

  // Verify error from REPL 1 went to REPL 1's output
  assert.match(output1.accumulator, /error from repl1/,
               'REPL 1 should have received its own async error');

  // Verify error from REPL 2 went to REPL 2's output
  assert.match(output2.accumulator, /error from repl2/,
               'REPL 2 should have received its own async error');

  // Verify errors did not cross over to wrong REPL
  assert.doesNotMatch(output1.accumulator, /error from repl2/,
                      'REPL 1 should not have received REPL 2\'s error');
  assert.doesNotMatch(output2.accumulator, /error from repl1/,
                      'REPL 2 should not have received REPL 1\'s error');
}), 100);
