'use strict';
const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('assert');

const originalProcessNewListenerCount = process.listenerCount('newListener');
const { replServer } = startNewREPLServer();

const listenerCountBeforeClose = process.listenerCount('newListener');
replServer.close();
replServer.once('exit', common.mustCall(() => {
  setImmediate(common.mustCall(() => {
    const listenerCountAfterClose = process.listenerCount('newListener');
    assert.strictEqual(listenerCountAfterClose, listenerCountBeforeClose - 1);
    assert.strictEqual(listenerCountAfterClose, originalProcessNewListenerCount);
  }));
}));
