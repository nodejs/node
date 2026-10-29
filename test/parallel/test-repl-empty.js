'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

let evalCalledWithExpectedArgs = false;

const { replServer } = startNewREPLServer({
  eval: common.mustCall((cmd, context) => {
    // Assertions here will not cause the test to exit with an error code
    // so set a boolean that is checked later instead.
    evalCalledWithExpectedArgs = (cmd === '\n');
  })
});

try {
  // Empty strings should be sent to the repl's eval function
  replServer.write('\n');
} finally {
  replServer.write('.exit\n');
}

assert(evalCalledWithExpectedArgs);
