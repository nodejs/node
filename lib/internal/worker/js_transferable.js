'use strict';
const { Error } = primordials;
const {
  messaging_deserialize_symbol,
  messaging_transfer_symbol,
  messaging_clone_symbol,
  messaging_transfer_list_symbol
} = internalBinding('symbols');
const {
  JSTransferable,
  setDeserializerCreateObjectFunction
} = internalBinding('messaging');

function setup() {
  // Register the handler that will be used when deserializing JS-based objects
  // from .postMessage() calls. The format of `deserializeInfo` is generally
  // 'module:Constructor', e.g. 'internal/fs/promises:FileHandle'.
  setDeserializerCreateObjectFunction((deserializeInfo) => {
    const [ module, ctor ] = deserializeInfo.split(':');
    const Ctor = require(module)[ctor];
    if (typeof Ctor !== 'function' ||
        !(Ctor.prototype instanceof JSTransferable)) {
      // Not one of the official errors because one should not be able to get
      // here without messing with Node.js internals.
      // eslint-disable-next-line no-restricted-syntax
      throw new Error(`Unknown deserialize spec ${deserializeInfo}`);
    }
    return new Ctor();
  });
}

module.exports = {
  setup,
  JSTransferable,
  kClone: messaging_clone_symbol,
  kDeserialize: messaging_deserialize_symbol,
  kTransfer: messaging_transfer_symbol,
  kTransferList: messaging_transfer_list_symbol
};
