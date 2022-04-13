'use strict';
require('../common');

process.setSourceMapsEnabled(true);

try {
  require('../fixtures/source-map/enclosing-call-site-min.js');
} catch (e) {
  console.log(e);
}

delete require.cache[require
  .resolve('../fixtures/source-map/enclosing-call-site-min.js')];

process.setSourceMapsEnabled(false);

require('../fixtures/source-map/enclosing-call-site-min.js');
