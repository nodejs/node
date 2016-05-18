'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

{
  let evalCalledWithExpectedArgs = false;

  const options = {
    eval: common.mustCall((cmd, context) => {
      // Assertions here will not cause the test to exit with an error code
      // so set a boolean that is checked in process.on('exit',...) instead.
      evalCalledWithExpectedArgs = (cmd === 'foo\n' && context.foo === 'bar');
    })
  };

  const r = repl.start(options);
  r.context = {foo: 'bar'};

  try {
    r.write('foo\n');
  } finally {
    r.write('.exit\n');
  }

  process.on('exit', () => {
    assert(evalCalledWithExpectedArgs);
  });
}
