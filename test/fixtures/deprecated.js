'use strict';
const util = require('util');
const deprecated = util.deprecate(() => {
  console.error('This is deprecated');
}, 'this function is deprecated');

deprecated();
