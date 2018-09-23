'use strict';
require('../common');
var assert = require('assert'),
    exception = null;

try {
  eval('"\\uc/ef"');
} catch (e) {
  exception = e;
}

assert(exception instanceof SyntaxError);
