// Flags: --experimental-require-module
'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/network-import.mjs');
}, {
  code: 'ERR_NETWORK_IMPORT_DISALLOWED'
});
