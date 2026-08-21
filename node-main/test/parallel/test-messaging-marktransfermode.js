// Flags: --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');
const { E, F } = require('internal/test/transfer');

// Tests that F is transferable even tho it does not directly,
// observably extend the JSTransferable class.

const mc = new MessageChannel();

mc.port1.onmessageerror = common.mustNotCall();

mc.port1.onmessage = common.mustCall(({ data }) => {
  assert(data instanceof F);
  assert(data instanceof E);
  assert.strictEqual(data.b, 1);
  mc.port1.close();
});

mc.port2.postMessage(new F(1));
