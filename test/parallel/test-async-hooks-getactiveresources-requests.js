'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { getActiveResources } = require('async_hooks');

for (let i = 0; i < 12; i++)
  fs.open(__filename, 'r', common.mustCall());

assert.strictEqual(Object.values(getActiveResources()).length, 12);
