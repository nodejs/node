'use strict';
require('../common');
process.domain = null;
var timer = setTimeout(function() {
  console.log('this console.log statement should not make node crash');
}, 1);
