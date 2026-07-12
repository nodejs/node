'use strict';

require('../common');

const assert = require('assert');

assert.strictEqual(typeof DOMException, 'function');

assert.throws(() => {
  atob('我要抛错！');
}, DOMException);
