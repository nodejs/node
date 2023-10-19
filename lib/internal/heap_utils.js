'use strict';
const {
  Symbol,
  Uint8Array,
} = primordials;
const {
  kUpdateTimer,
  onStreamRead,
} = require('internal/stream_base_commons');
const { owner_symbol } = require('internal/async_hooks').symbols;
const { Readable } = require('stream');
const { validateObject, validateBoolean } = require('internal/validators');
const { kEmptyObject } = require('internal/util');

const kHandle = Symbol('kHandle');

function getHeapSnapshotOptions(options = kEmptyObject) {
  validateObject(options, 'options');
  const {
    exposeInternals = false,
    exposeNumericValues = false,
  } = options;
  validateBoolean(exposeInternals, 'options.exposeInternals');
  validateBoolean(exposeNumericValues, 'options.exposeNumericValues');
  return new Uint8Array([+exposeInternals, +exposeNumericValues]);
}

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
  getHeapSnapshotOptions,
  HeapSnapshotStream,
};
