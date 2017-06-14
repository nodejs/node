'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();

hooks.enable();
const Zlib = process.binding('zlib').Zlib;
const constants = process.binding('constants').zlib;

const handle = new Zlib(constants.DEFLATE);

const as = hooks.activitiesOfTypes('ZLIB');
assert.strictEqual(as.length, 1);
const hdl = as[0];
assert.strictEqual(hdl.type, 'ZLIB');
assert.strictEqual(typeof hdl.uid, 'number');
assert.strictEqual(typeof hdl.triggerAsyncId, 'number');
checkInvocations(hdl, { init: 1 }, 'when created handle');

handle.init(
  constants.Z_DEFAULT_WINDOWBITS,
  constants.Z_MIN_LEVEL,
  constants.Z_DEFAULT_MEMLEVEL,
  constants.Z_DEFAULT_STRATEGY,
  Buffer.from('')
);
checkInvocations(hdl, { init: 1 }, 'when initialized handle');

const inBuf = Buffer.from('x');
const outBuf = Buffer.allocUnsafe(1);

let count = 2;
handle.callback = common.mustCall(onwritten, 2);
handle.write(true, inBuf, 0, 1, outBuf, 0, 1);
checkInvocations(hdl, { init: 1 }, 'when invoked write() on handle');

function onwritten() {
  if (--count) {
    // first write
    checkInvocations(hdl, { init: 1, before: 1 },
                     'when wrote to handle the first time');
    handle.write(true, inBuf, 0, 1, outBuf, 0, 1);
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
}
