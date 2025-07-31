'use strict';

const common = require('../common');
const repl = require('node:repl');
const stream = require('node:stream');
const assert = require('node:assert');

// This test checks that an eval function returning an error in its callback
// after the repl server has been closed doesn't cause an ERR_USE_AFTER_CLOSE
// error to be thrown (reference: https://github.com/nodejs/node/issues/58784)

(async () => {
  const close$ = Promise.withResolvers();
  const eval$ = Promise.withResolvers();

  const input = new stream.PassThrough();
  const output = new stream.PassThrough();

  const replServer = repl.start({
    input,
    output,
    eval(_cmd, _context, _file, cb) {
      close$.promise.then(() => {
        cb(new Error('Error returned from the eval callback'));
        eval$.resolve();
      });
    },
  });

  let outputStr = '';
  output.on('data', (data) => {
    outputStr += data;
  });

  input.write('\n');

  replServer.close();
  close$.resolve();

  process.on('uncaughtException', common.mustNotCall());

  await eval$.promise;

  assert.match(outputStr, /Uncaught Error: Error returned from the eval callback/);
})().then(common.mustCall());
