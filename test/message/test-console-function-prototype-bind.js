'use strict';
require('../common');

Function.prototype.bind = function() {
  console.log('Should be logged to stdout.');
};

(() => {}).bind();
