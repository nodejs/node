// Flags: --enable-source-maps

'use strict';
require('../common');
Error.stackTraceLimit = 5;

process.setSourceMapsEnabled(false);

try {
  require('../fixtures/source-map/enclosing-call-site-min.js');
} catch (e) {
  console.log(e);
}

delete require.cache[require
  .resolve('../fixtures/source-map/enclosing-call-site-min.js')];

// Re-enable.
process.setSourceMapsEnabled(true);

try {
  require('../fixtures/source-map/enclosing-call-site-min.js');
} catch (e) {
  console.log(e);
}
