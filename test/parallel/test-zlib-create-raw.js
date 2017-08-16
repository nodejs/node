'use strict';

require('../common');
const assert = require('assert');
const zlib = require('zlib');

{
  const inflateRaw = zlib.createInflateRaw();
  assert(inflateRaw instanceof zlib.InflateRaw);
}

{
  const deflateRaw = zlib.createDeflateRaw();
  assert(deflateRaw instanceof zlib.DeflateRaw);
}
