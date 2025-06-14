'use strict';

const common = require('../common');
const assert = require('assert');
const {
  rmdir,
  rmdirSync,
  promises: { rmdir: rmdirPromise }
} = require('fs');

assert.throws(() => {
  rmdir('nonexistent', {
    recursive: true,
  }, common.mustNotCall());
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

assert.throws(() => {
  rmdirSync('nonexistent', {
    recursive: true,
  });
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

rmdirPromise('nonexistent', {
  recursive: true,
}).then(common.mustNotCall(), common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
}));
