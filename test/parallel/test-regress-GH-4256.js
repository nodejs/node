'use strict';
require('../common');
process.domain = null;
setTimeout(function() {
  console.log('this console.log statement should not make node crash');
}, 1);
