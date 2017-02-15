'use strict';
require('../common');
const assert = require('assert');

assert.doesNotThrow(function() {
  require('vm').runInNewContext('"use strict"; var v = 1; v = 2');
});
