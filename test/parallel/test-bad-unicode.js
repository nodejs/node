'use strict';
const assert = require('assert');
var exception = null;

try {
  eval('"\\uc/ef"');
} catch (e) {
  exception = e;
}

assert(exception instanceof SyntaxError);
