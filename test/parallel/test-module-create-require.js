'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

const { createRequire } = require('module');

const u = fixtures.fileURL('fake.js');

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
