// Flags: --expose-gc
'use strict';
const common = require('../common');
const onGC = require('../common/ongc');
const assert = require('assert');
const zlib = require('zlib');

// Checks that, if a zlib context fails with an error, it can still be GC'ed:
// Refs: https://github.com/nodejs/node/issues/22705

const ongc = common.mustCall();

{
  const input = Buffer.from('foobar');
  const strm = zlib.createInflate();
  strm.end(input);
  strm.once('error', common.mustCall((err) => assert(err)));
  onGC(strm, { ongc });
}

setImmediate(() => global.gc());
