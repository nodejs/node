'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

const { createRequireFromPath } = require('module');

const p = path.resolve(__dirname, '..', 'fixtures', 'fake.js');

const req = createRequireFromPath(p);
assert.strictEqual(req('./baz'), 'perhaps I work');
