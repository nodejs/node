const LEN      = 1e7;
const noAssert = process.argv[3] == 'true' ? true
                  : process.argv[3] == 'false' ? false
                  : undefined;

var timer = require('./_bench_timer');

var buff = (process.argv[2] == 'slow') ?
      (new require('buffer').SlowBuffer(8)) :
      (new Buffer(8));
var i;

buff.writeDoubleLE(0, 0, noAssert);

timer('readUInt8', function() {
  for (i = 0; i < LEN; i++) {
    buff.readUInt8(0, noAssert);
  }
});

timer('readUInt16LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readUInt16LE(0, noAssert);
  }
});

timer('readUInt16BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readUInt16BE(0, noAssert);
  }
});

timer('readUInt32LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readUInt32LE(0, noAssert);
  }
});

timer('readUInt32BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readUInt32BE(0, noAssert);
  }
});

timer('readInt8', function() {
  for (i = 0; i < LEN; i++) {
    buff.readInt8(0, noAssert);
  }
});

timer('readInt16LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readInt16LE(0, noAssert);
  }
});

timer('readInt16BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readInt16BE(0, noAssert);
  }
});

timer('readInt32LE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readInt32LE(0, noAssert);
  }
});

timer('readInt32BE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readInt32BE(0, noAssert);
  }
});

timer('readFloatLE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readFloatLE(0, noAssert);
  }
});

timer('readFloatBE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readFloatBE(0, noAssert);
  }
});

timer('readDoubleLE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readDoubleLE(0, noAssert);
  }
});

timer('readDoubleBE', function() {
  for (i = 0; i < LEN; i++) {
    buff.readDoubleBE(0, noAssert);
  }
});
