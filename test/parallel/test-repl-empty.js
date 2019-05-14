'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

{
  let evalCalledWithExpectedArgs = false;

  const options = {
    eval: common.mustCall((cmd, context) => {
      // Assertions here will not cause the test to exit with an error code
      // so set a boolean that is checked later instead.
      evalCalledWithExpectedArgs = (cmd === '\n');
    })
  };

  const r = repl.start(options);

  try {
    // Empty strings should be sent to the repl's eval function
    r.write('\n');
  } finally {
    r.write('.exit\n');
  }

  assert(evalCalledWithExpectedArgs);
}
