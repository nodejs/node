const LEN    = 1e7;

const INT8   = 0x7f;
const INT16  = 0x7fff;
const INT32  = 0x7fffffff;
const UINT8  = INT8 * 2;
const UINT16 = INT16 * 2;
const UINT32 = INT32 * 2;

const noAssert = process.argv[3] == 'true' ? true
                  : process.argv[3] == 'false' ? false
                  : undefined;

var timer = require('./_bench_timer');

var buff = (process.argv[2] == 'slow') ?
      (new require('buffer').SlowBuffer(8)) :
      (new Buffer(8));
var i;

timer('writeUInt8', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeUInt8(i % UINT8, 0, noAssert);
  }
});

timer('writeUInt16LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeUInt16LE(i % UINT16, 0, noAssert);
  }
});

timer('writeUInt16BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeUInt16BE(i % UINT16, 0, noAssert);
  }
});

timer('writeUInt32LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeUInt32LE(i % UINT32, 0, noAssert);
  }
});

timer('writeUInt32BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeUInt32BE(i % UINT32, 0, noAssert);
  }
});

timer('writeInt8', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeInt8(i % INT8, 0, noAssert);
  }
});

timer('writeInt16LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeInt16LE(i % INT16, 0, noAssert);
  }
});

timer('writeInt16BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeInt16BE(i % INT16, 0, noAssert);
  }
});

timer('writeInt32LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeInt32LE(i % INT32, 0, noAssert);
  }
});

timer('writeInt32BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeInt32BE(i % INT32, 0, noAssert);
  }
});

timer('writeFloatLE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeFloatLE(i * 0.1, 0, noAssert);
  }
});

timer('writeFloatBE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeFloatBE(i * 0.1, 0, noAssert);
  }
});

timer('writeDoubleLE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeDoubleLE(i * 0.1, 0, noAssert);
  }
});

timer('writeDoubleBE', function() {
  for (i = 0; i < LEN; i++) {
    buff.writeDoubleBE(i * 0.1, 0, noAssert);
  }
});
