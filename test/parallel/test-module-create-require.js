'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

const util = require('util');
util.inspect.defaultOptions.depth = 4;

const { createRequireFromPath } = require('module');

const p = path.join(process.cwd(), 'test/fixtures/fake.js');
const p2 = './test/fixtures/fake.js';
const p3 = './fixtures/fake.js';

const req = createRequireFromPath(p);
const req2 = createRequireFromPath(p2);
const req3 = createRequireFromPath(p3);
process.chdir('./test/');
const req4 = createRequireFromPath(p3);

assert.strictEqual(req('./baz'), 'perhaps I work');
assert.strictEqual(req2('./baz'), 'perhaps I work');
// TODO: req3('./baz') should throw an error. There's some caching involved.
assert.strictEqual(req3('./baz'), 'perhaps I work');
assert.strictEqual(req4('./baz'), 'perhaps I work');
