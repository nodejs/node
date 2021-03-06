// Flags: --expose-internals --no-warnings
'use strict';
const common = require('../common');
const assert = require('assert');
const {
  JSTransferable, kTransfer, kTransferList
} = require('internal/worker/js_transferable');
const { MessageChannel } = require('worker_threads');

// Transferring a JSTransferable that refers to another, untransferable, value
// in its transfer list should not crash hard.

class OuterTransferable extends JSTransferable {
  constructor() {
    super();
    // Create a detached MessagePort at this.inner
    const c = new MessageChannel();
    this.inner = c.port1;
    c.port2.postMessage(this.inner, [ this.inner ]);
  }

  [kTransferList] = common.mustCall(() => {
    return [ this.inner ];
  });

  [kTransfer] = common.mustCall(() => {
    return {
      data: { inner: this.inner },
      deserializeInfo: 'does-not:matter'
    };
  });
}

const { port1 } = new MessageChannel();
const ot = new OuterTransferable();
assert.throws(() => {
  port1.postMessage(ot, [ot]);
}, { name: 'DataCloneError' });
