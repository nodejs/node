'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');
const DEFAULT_MAX_LISTENERS = require('events').defaultMaxListeners;

ArrayStream.prototype.write = () => {};

const putIn = new ArrayStream();
const testMe = repl.start('', putIn);

// https://github.com/nodejs/node/issues/18284
// Tab-completion should not repeatedly add the
// `Runtime.executionContextCreated` listener
process.on('warning', common.mustNotCall());

putIn.run(['async function test() {']);
for (let i = 0; i < DEFAULT_MAX_LISTENERS; i++) {
  testMe.complete('await Promise.resolve()', () => {});
}
