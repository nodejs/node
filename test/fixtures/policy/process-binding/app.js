'use strict';

const assert = require('assert');

assert.throws(() => { process.binding(); }, {
  code: 'ERR_ACCESS_DENIED'
});
assert.throws(() => { process._linkedBinding(); }, {
  code: 'ERR_ACCESS_DENIED'
});
