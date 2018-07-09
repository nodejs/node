'use strict';

// Flags: --experimental-modules

const common = require('../common');
const assert = require('assert');

common.crashOnUnhandledRejection();

const re = /^The namespace of .+? is thenable\. See https:\/\/mdn\.io\/thenable for more information\.$/;
process.on('warning', common.mustCall((warning) => {
  assert(re.test(warning.message));
}));

import('../fixtures/es-modules/thenable.mjs');
