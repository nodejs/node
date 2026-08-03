'use strict';

require('../common');

const assert = require('assert');
const { compileFunction } = require('node:vm');

const min = -2147483648;
const max = 2147483647;

compileFunction('', [], { lineOffset: min, columnOffset: min });
compileFunction('', [], { lineOffset: max, columnOffset: max });

assert.throws(
  () => {
    compileFunction('', [], { lineOffset: min - 1, columnOffset: max });
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: /The value of "options\.lineOffset" is out of range/,
  }
);

assert.throws(
  () => {
    compileFunction('', [], { lineOffset: min, columnOffset: min - 1 });
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: /The value of "options\.columnOffset" is out of range/,
  }
);
