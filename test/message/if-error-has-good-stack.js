'use strict';

require('../common');
const assert = require('assert');

let err;
// Create some random error frames.
(function a() {
  (function b() {
    (function c() {
      err = new Error('test error');
    })();
  })();
})();

(function x() {
  (function y() {
    (function z() {
      assert.ifError(err);
    })();
  })();
})();
