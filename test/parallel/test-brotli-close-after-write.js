// Flags: --expose-brotli
'use strict';
const common = require('../common');
const brotli = require('brotli');

brotli.compress('hello', common.mustCall(function(err, out) {
  const decompress = new brotli.Decompress();
  decompress.write(out);
  decompress.close(common.mustCall());
}));
