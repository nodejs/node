'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

function run({code, validate}) {
  const putIn = new common.ArrayStream();
  let error;
  putIn.write = (output) => (error += output);
  const replServer = repl.start({
    prompt: '',
    input: putIn,
    output: putIn,
    terminal: true
  });
  putIn.run(code);
  replServer.forceExecute();
  validate(error);
}

const tests = [{
  code: [
    'var z y;'
  ],
  validate: (data) => {
    assert.strictEqual(/^\s*\^\s*$/m.test(data), true);
    assert.strictEqual(/^SyntaxError/m.test(data), true);
  }
}, {
  code: [
    'throw new Error("tiny stack");'
  ],
  validate: (data) => {
    assert.strictEqual(/Error/.test(data), true);
    assert.strictEqual(/REPLServer/.test(data), false);
    assert.strictEqual(/vm\.js/.test(data), false);

    run({
      code: ['new Error("tiny stack");'],
      validate: common.mustCall((fullStack) => {
        const lines = fullStack.split('\n').length;
        assert.strictEqual(lines > data.split('\n').length, true);
      })
    });
  }
}];

tests.forEach(run);
