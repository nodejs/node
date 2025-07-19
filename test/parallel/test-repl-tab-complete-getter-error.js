'use strict';

const common = require('../common');
const repl = require('repl');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');

(async function() {
  await runTest();
})().then(common.mustCall());

async function runTest() {
  const input = new ArrayStream();
  const output = new ArrayStream();

  const replServer = repl.start({
    prompt: '',
    input,
    output: output,
    allowBlockingCompletions: true,
    terminal: true
  });

  replServer._domain.on('error', (e) => {
    assert.fail(`Error in REPL domain: ${e}`);
  });

  await new Promise((resolve, reject) => {
    replServer.eval(`
     const getNameText = () => "name";
     const foo = { get name() { throw new Error(); } };
     `, replServer.context, '', (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });

  ['foo.name.', 'foo["name"].', 'foo[getNameText()].'].forEach((test) => {
    replServer.complete(
      test,
      common.mustCall((error, data) => {
        assert.strictEqual(error, null);
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], test);
      })
    );
  });
}
