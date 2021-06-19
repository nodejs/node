// Flags: --enable-source-maps

'use strict';
require('../common');

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

require('../fixtures/source-map/enclosing-call-site-min.js');
