// Flags:  --enable-source-maps

'use strict';
require('../../../common');
Error.stackTraceLimit = 2;

try {
  require('../typescript-sourcemapping_url_string');
} catch (err) {
  console.info(err);
}
