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
var dv = new DataView(buff);
var i;

timer('setUint8', function() {
  for (i = 0; i < LEN; i++) {
    dv.setUint8(0, i % UINT8);
  }
});

timer('setUint16 - LE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setUint16(0, i % UINT16, true);
  }
});

timer('setUint16 - BE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setUint16(0, i % UINT16);
  }
});

timer('setUint32 - LE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setUint32(0, i % UINT32, true);
  }
});

timer('setUint32 - BE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setUint32(0, i % UINT32);
  }
});

timer('setInt8', function() {
  for (i = 0; i < LEN; i++) {
    dv.setInt8(0, i % INT8);
  }
});

timer('setInt16 - LE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setInt16(0, i % INT16, true);
  }
});

timer('setInt16 - BE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setInt16(0, i % INT16);
  }
});

timer('setInt32 - LE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setInt32(0, i % INT32, true);
  }
});

timer('setInt32 - BE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setInt32(0, i % INT32);
  }
});

timer('setFloat32 - LE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setFloat32(0, i * 0.1, true);
  }
});

timer('setFloat32 - BE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setFloat32(0, i * 0.1);
  }
});

timer('setFloat64 - LE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setFloat64(0, i * 0.1, true);
  }
});

timer('setFloat64 - BE', function() {
  for (i = 0; i < LEN; i++) {
    dv.setFloat64(0, i * 0.1);
  }
});
