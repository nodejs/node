'use strict';

require('../common');

const assert = require('assert');

setTimeout(() => {}, 0);

assert.deepStrictEqual(process.getActiveResourcesInfo(), ['Timeout']);
