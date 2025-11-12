'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

(async function() {
  await runTest();
})().then(common.mustCall());

async function runTest() {
  const { replServer } = startNewREPLServer();

  await new Promise((resolve, reject) => {
    replServer.eval(`
     const foo = { get name() { throw new Error(); } };
     `, replServer.context, '', (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });

  ['foo.name.', 'foo["name"].'].forEach((test) => {
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
