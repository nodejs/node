'use strict';
require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const testingReplPrompt = '_REPL_TESTING_PROMPT_>';

const { replServer, output } = startNewREPLServer(
  { prompt: testingReplPrompt },
  { disableDomainErrorAssert: true }
);

replServer.write('throw new Error("foo[a]")\n');

assert.strictEqual(
  output.accumulator.split('\n').filter((line) => !line.includes(testingReplPrompt)).join(''),
  'Uncaught Error: foo[a]'
);
