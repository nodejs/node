'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const { getActiveResources } = require('async_hooks');

for (let i = 0; i < 12; i++)
  fs.open(__filename, 'r', () => {});

assert.strictEqual(12, Object.values(getActiveResources()).length);
