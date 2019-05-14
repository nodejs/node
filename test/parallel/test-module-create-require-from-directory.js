'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

const { createRequireFromPath } = require('module');

const fixPath = path.resolve(__dirname, '..', 'fixtures');
const p = path.join(fixPath, path.sep);

const req = createRequireFromPath(p);
const reqFromNotDir = createRequireFromPath(fixPath);

assert.strictEqual(req('./baz'), 'perhaps I work');
assert.throws(() => {
  reqFromNotDir('./baz');
}, { code: 'MODULE_NOT_FOUND' });
