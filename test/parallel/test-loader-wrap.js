// Flags: --expose-internals
'use strict';

require('../common');
const Module = require('module');
const assert = require('assert');

Module.wrap = function() {
  return '(function(){ throw new Error("Error")});';
};

assert.throws(() => require('../fixtures/foo'), { message: 'Error' });
