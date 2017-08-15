'use strict';
require('../common');
const assert = require('assert');

// testing basic buffer read functions
const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf, 0xff, 0xd9, 0x01, 0xde]);

function read(buff, funx, args, expected) {

  assert.strictEqual(buff[funx](...args), expected);
  assert.throws(
    () => buff[funx](-1),
    /^RangeError: Index out of range$/
  );

  assert.doesNotThrow(
    () => assert.strictEqual(buff[funx](...args, true), expected),
    'noAssert does not change return value for valid ranges'
  );

}

// testing basic functionality of readDoubleBE() and readDOubleLE()
read(buf, 'readDoubleBE', [1], -3.1827727774563287e+295);
read(buf, 'readDoubleLE', [1], -6.966010051009108e+144);

// testing basic functionality of readFLoatBE() and readFloatLE()
read(buf, 'readFloatBE', [1], -1.6691549692541768e+37);
read(buf, 'readFloatLE', [1], -7861303808);

// testing basic functionality of readInt8()
read(buf, 'readInt8', [1], -3);

// testing basic functionality of readInt16BE() and readInt16LE()
read(buf, 'readInt16BE', [1], -696);
read(buf, 'readInt16LE', [1], 0x48fd);

// testing basic functionality of readInt32BE() and readInt32LE()
read(buf, 'readInt32BE', [1], -45552945);
read(buf, 'readInt32LE', [1], -806729475);

// testing basic functionality of readIntBE() and readIntLE()
read(buf, 'readIntBE', [1, 1], -3);
read(buf, 'readIntLE', [2, 1], 0x48);

// testing basic functionality of readUInt8()
read(buf, 'readUInt8', [1], 0xfd);

// testing basic functionality of readUInt16BE() and readUInt16LE()
read(buf, 'readUInt16BE', [2], 0x48ea);
read(buf, 'readUInt16LE', [2], 0xea48);

// testing basic functionality of readUInt32BE() and readUInt32LE()
read(buf, 'readUInt32BE', [1], 0xfd48eacf);
read(buf, 'readUInt32LE', [1], 0xcfea48fd);

// testing basic functionality of readUIntBE() and readUIntLE()
read(buf, 'readUIntBE', [2, 0], 0xfd);
read(buf, 'readUIntLE', [2, 0], 0x48);
