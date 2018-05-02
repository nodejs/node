// Flags: --expose-brotli
'use strict';
const common = require('../common');
const brotli = require('brotli');

brotli.compress('hello', common.mustCall(function(err, out) {
  const unzip = new brotli.Decompress();
  unzip.close(common.mustCall());
  common.expectsError(
    () => unzip.write(out),
    {
      code: 'ERR_STREAM_DESTROYED',
      type: Error,
      message: 'Cannot call write after a stream was destroyed'
    }
  );
}));
