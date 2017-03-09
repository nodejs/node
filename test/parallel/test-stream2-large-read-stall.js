'use strict';
const common = require('../common');
const assert = require('assert');

// If everything aligns so that you do a read(n) of exactly the
// remaining buffer, then make sure that 'end' still emits.

const READSIZE = 100;
const PUSHSIZE = 20;
const PUSHCOUNT = 1000;
const HWM = 50;

const Readable = require('stream').Readable;
const r = new Readable({
  highWaterMark: HWM
});
const rs = r._readableState;

r._read = push;

r.on('readable', function() {
  console.error('>> readable');
  let ret;
  do {
    console.error('  > read(%d)', READSIZE);
    ret = r.read(READSIZE);
    console.error('  < %j (%d remain)', ret && ret.length, rs.length);
  } while (ret && ret.length === READSIZE);

  console.error('<< after read()',
                ret && ret.length,
                rs.needReadable,
                rs.length);
});

r.on('end', common.mustCall(function() {
  assert.strictEqual(pushes, PUSHCOUNT + 1);
}));

let pushes = 0;
function push() {
  if (pushes > PUSHCOUNT)
    return;

  if (pushes++ === PUSHCOUNT) {
    console.error('   push(EOF)');
    return r.push(null);
  }

  console.error('   push #%d', pushes);
  if (r.push(Buffer.allocUnsafe(PUSHSIZE)))
    setTimeout(push, 1);
}
