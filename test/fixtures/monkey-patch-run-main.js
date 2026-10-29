'use strict';

const oldRunMain = require('module').runMain;

require('module').runMain = function(...args) {
  console.log('runMain is monkey patched!');
  oldRunMain(...args);
};
