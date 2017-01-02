'use strict';
const common = require('../common');
var assert = require('assert');

// If everything aligns so that you do a read(n) of exactly the
// remaining buffer, then make sure that 'end' still emits.

var READSIZE = 100;
var PUSHSIZE = 20;
var PUSHCOUNT = 1000;
var HWM = 50;

var Readable = require('stream').Readable;
var r = new Readable({
  highWaterMark: HWM
});
var rs = r._readableState;

r._read = push;

r.on('readable', function() {
  console.error('>> readable');
  do {
    console.error('  > read(%d)', READSIZE);
    var ret = r.read(READSIZE);
    console.error('  < %j (%d remain)', ret && ret.length, rs.length);
  } while (ret && ret.length === READSIZE);

  console.error('<< after read()',
                ret && ret.length,
                rs.needReadable,
                rs.length);
});

r.on('end', common.mustCall(function() {}));

var pushes = 0;
function push() {
  if (pushes > PUSHCOUNT)
    return;

  if (pushes++ === PUSHCOUNT) {
    console.error('   push(EOF)');
    return r.push(null);
  }

  console.error('   push #%d', pushes);
  if (r.push(Buffer.allocUnsafe(PUSHSIZE)))
    setTimeout(push);
}

process.on('exit', function() {
  assert.equal(pushes, PUSHCOUNT + 1);
});
