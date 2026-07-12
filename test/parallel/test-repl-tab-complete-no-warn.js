'use strict';

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const DEFAULT_MAX_LISTENERS = require('events').defaultMaxListeners;

const { replServer, input } = startNewREPLServer();

// https://github.com/nodejs/node/issues/18284
// Tab-completion should not repeatedly add the
// `Runtime.executionContextCreated` listener
process.on('warning', common.mustNotCall());

input.run(['async function test() {']);
for (let i = 0; i < DEFAULT_MAX_LISTENERS; i++) {
  replServer.complete('await Promise.resolve()', () => {});
}
