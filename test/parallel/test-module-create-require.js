'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

const { createRequire, createRequireFromPath } = require('module');

const p = path.resolve(__dirname, '..', 'fixtures', 'fake.js');
const u = new URL(`file://${p}`);

const req = createRequireFromPath(p);
assert.strictEqual(req('./baz'), 'perhaps I work');

const reqToo = createRequire(u);
assert.deepStrictEqual(reqToo('./experimental'), { ofLife: 42 });

assert.throws(() => {
  createRequire('https://github.com/nodejs/node/pull/27405/');
}, {
  code: 'ERR_INVALID_ARG_VALUE'
});

assert.throws(() => {
  createRequire('../');
}, {
  code: 'ERR_INVALID_ARG_VALUE'
});

assert.throws(() => {
  createRequire({});
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  message: 'The argument \'filename\' must be a file URL object, file URL ' +
           'string, or absolute path string. Received {}'
});
