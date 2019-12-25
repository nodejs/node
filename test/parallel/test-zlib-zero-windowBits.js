'use strict';

require('../common');
const assert = require('assert');
const zlib = require('zlib');


// windowBits is a special case in zlib. On the compression side, 0 is invalid.
// On the decompression side, it indicates that zlib should use the value from
// the header of the compressed stream.
{
  const inflate = zlib.createInflate({ windowBits: 0 });
  assert(inflate instanceof zlib.Inflate);
}

{
  const gunzip = zlib.createGunzip({ windowBits: 0 });
  assert(gunzip instanceof zlib.Gunzip);
}

{
  const unzip = zlib.createUnzip({ windowBits: 0 });
  assert(unzip instanceof zlib.Unzip);
}

{
  assert.throws(() => zlib.createGzip({ windowBits: 0 }), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.windowBits" is out of range. ' +
             'It must be >= 8 and <= 15. Received 0'
  });
}
