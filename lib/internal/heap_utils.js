'use strict';
const {
  Symbol
} = primordials;
const {
  kUpdateTimer,
  onStreamRead,
} = require('internal/stream_base_commons');
const { owner_symbol } = require('internal/async_hooks').symbols;
const { Readable } = require('stream');

const kHandle = Symbol('kHandle');

class HeapSnapshotStream extends Readable {
  constructor(handle) {
    super({ autoDestroy: true });
    this[kHandle] = handle;
    handle[owner_symbol] = this;
    handle.onread = onStreamRead;
  }

  _read() {
    if (this[kHandle])
      this[kHandle].readStart();
  }

  _destroy() {
    // Release the references on the handle so that
    // it can be garbage collected.
    this[kHandle][owner_symbol] = undefined;
    this[kHandle] = undefined;
  }

  [kUpdateTimer]() {
    // Does nothing
  }
}

module.exports = {
  HeapSnapshotStream
};
