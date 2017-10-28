'use strict';
require('../common');

// This test makes sures that the repl does not 
// crash or emit error when throwing `null|undefined`
// ie `throw null` or `throw undefined`

const assert = require('assert');
const repl = require('repl');

const r = repl.start();

r.write('throw null\n');
r.write('throw undefined\n');
r.write('.exit\n');

let i = 0;
r.on('line', function replOutput(output) {
  const testStatement = i === 0 ? 'null' : 'undefined';
  const expectedOutput = 'Thrown: ' + testStatement;
  const msg = 'repl did not throw ' + testStatement;
  assert.deepStrictEqual(output, expectedOutput, msg);
  i++;
});
