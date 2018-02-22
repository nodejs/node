'use strict';

const common = require('../common');
const repl = require('repl');
const DEFAULT_MAX_LISTENERS = require('events').defaultMaxListeners;

common.ArrayStream.prototype.write = () => {
};

const putIn = new common.ArrayStream();
const testMe = repl.start('', putIn);

// https://github.com/nodejs/node/issues/18284
// Tab-completion should not repeatedly add the
// `Runtime.executionContextCreated` listener
process.on('warning', common.mustNotCall());

putIn.run(['.clear']);
putIn.run(['async function test() {']);
for (let i = 0; i < DEFAULT_MAX_LISTENERS; i++) {
  testMe.complete('await Promise.resolve()', () => {});
}
