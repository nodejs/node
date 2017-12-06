// Flags: --expose-gc
'use strict';

// Regression test for https://github.com/nodejs/node/issues/17475
// Unfortunately, this tests only "works" reliably when checked with valgrind or
// a similar tool.

/* eslint-disable no-unused-vars */

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { TLSSocket } = require('tls');
const makeDuplexPair = require('../common/duplexpair');

let { clientSide } = makeDuplexPair();

let clientTLS = new TLSSocket(clientSide, { isServer: false });
let clientTLSHandle = clientTLS._handle;

setImmediate(() => {
  clientTLS = null;
  global.gc();
  clientTLSHandle = null;
  global.gc();
  setImmediate(() => {
    clientSide = null;
    global.gc();
  });
});
