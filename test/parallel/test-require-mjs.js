'use strict';
require('../common');
const assert = require('assert');

assert.equal(require('../fixtures/es-modules/test-esm-ok.mjs').default, true);
