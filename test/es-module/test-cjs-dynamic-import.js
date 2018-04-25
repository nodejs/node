'use strict';

// Flags: --experimental-modules

require('../common').crashOnUnhandledRejection();
const assert = require('assert');

import('assert').then((ns) => {
  assert.strictEqual(ns.default.ok, assert.ok);
});
