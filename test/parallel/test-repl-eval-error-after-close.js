'use strict';

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('node:assert');

// This test checks that an eval function returning an error in its callback
// after the repl server has been closed doesn't cause an ERR_USE_AFTER_CLOSE
// error to be thrown (reference: https://github.com/nodejs/node/issues/58784)

(async () => {
  const close$ = Promise.withResolvers();
  const eval$ = Promise.withResolvers();

  const { replServer, output } = startNewREPLServer({
    eval(_cmd, _context, _file, cb) {
      close$.promise.then(() => {
        cb(new Error('Error returned from the eval callback'));
        eval$.resolve();
      });
    },
  }, {
    disableDomainErrorAssert: true,
  });

  replServer.write('\n');

  replServer.close();
  close$.resolve();

  process.on('uncaughtException', common.mustNotCall());

  await eval$.promise;

  assert.match(output.accumulator, /Uncaught Error: Error returned from the eval callback/);
})().then(common.mustCall());
