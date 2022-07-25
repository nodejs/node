'use strict';

const {
  makeTransferable,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

process.emitWarning(
  'These APIs are for internal testing only. Do not use them.',
  'internal/test/transfer');

// Used as part of parallel/test-messaging-maketransferable.
// This has to exist within the lib/internal/ path in order
// for deserialization to work.

class E {
  constructor(b) {
    this.b = b;
  }
}

class F extends E {
  constructor(b) {
    super(b);
    /* eslint-disable-next-line no-constructor-return */
    return makeTransferable(this);
  }

  [kClone]() {
    return {
      data: { b: this.b },
      deserializeInfo: 'internal/test/transfer:F'
    };
  }

  [kDeserialize]({ b }) {
    this.b = b;
  }
}

module.exports = { E, F };
