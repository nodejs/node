'use strict';
const common = require('../common');

if (!process.execArgv.includes('--pending-deprecation'))
  common.requireFlags(['--pending-deprecation']);

const assert = require('assert');

common.expectWarning(
  'DeprecationWarning',
  'Assigning any value other than a string, number, or boolean to a ' +
  'process.env property is deprecated. Please make sure to convert the value ' +
  'to a string before setting process.env with it.',
  'DEP0104'
);

process.env.ABC = undefined;
assert.strictEqual(process.env.ABC, 'undefined');
