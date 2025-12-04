// Flags: --enable-source-maps --experimental-transform-types --no-warnings

require('../../../common');
const assert = require('node:assert');

enum Bar {
  makeSureTransformTypes,
}

try {
  assert(false);
} catch (e) {
  console.log(e);
}
