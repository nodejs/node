// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();

hooks.enable();
const { internalBinding } = require('internal/test/binding');
const { Zlib } = internalBinding('zlib');
const constants = require('zlib').constants;

const handle = new Zlib(constants.DEFLATE);

const as = hooks.activitiesOfTypes('ZLIB');
assert.strictEqual(as.length, 1);
const hdl = as[0];
assert.strictEqual(hdl.type, 'ZLIB');
assert.strictEqual(typeof hdl.uid, 'number');
assert.strictEqual(typeof hdl.triggerAsyncId, 'number');
checkInvocations(hdl, { init: 1 }, 'when created handle');

// Store all buffers together so that they do not get
// garbage collected.
const buffers = {
  writeResult: new Uint32Array(2),
  dictionary: new Uint8Array(0),
  inBuf: new Uint8Array([0x78]),
  outBuf: new Uint8Array(1),
};

handle.init(
  constants.Z_DEFAULT_WINDOWBITS,
  constants.Z_MIN_LEVEL,
  constants.Z_DEFAULT_MEMLEVEL,
  constants.Z_DEFAULT_STRATEGY,
  buffers.writeResult,
  function processCallback() { this.cb(); },
  buffers.dictionary,
);
checkInvocations(hdl, { init: 1 }, 'when initialized handle');

let count = 2;
handle.cb = common.mustCall(onwritten, 2);
handle.write(true, buffers.inBuf, 0, 1, buffers.outBuf, 0, 1);
checkInvocations(hdl, { init: 1 }, 'when invoked write() on handle');

function onwritten() {
  if (--count) {
    // first write
    checkInvocations(hdl, { init: 1, before: 1 },
                     'when wrote to handle the first time');
    handle.write(true, buffers.inBuf, 0, 1, buffers.outBuf, 0, 1);
  } else {
    // second write
    checkInvocations(hdl, { init: 1, before: 2, after: 1 },
                     'when wrote to handle the second time');
  }
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('ZLIB');
  // TODO: destroy never called here even with large amounts of ticks
  // is that correct?
  checkInvocations(hdl, { init: 1, before: 2, after: 2 }, 'when process exits');

  // Do something with `buffers` to keep them alive until here.
  buffers.buffers = buffers;
}
