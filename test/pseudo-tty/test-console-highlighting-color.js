'use strict';
require('../common');
const assert = require('assert');
const { Console } = require('console');

// Work with 16 colors
process.env.FORCE_COLOR = 1;
delete process.env.NODE_DISABLE_COLORS;
delete process.env.NO_COLOR;

// Arguments validations
const invalidTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: /must be of type/,
};
const mustBeOneOfError = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
  message: /must be one of/,
};
const options = {
  stdout: process.stdout,
  stderr: process.stderr,
  ignoreErrors: true,
};

options.highlight = { warn: 'abc' };
assert.throws(() => { new Console(options); }, invalidTypeError);

options.highlight = { warn: { bgColor: 'abc' } };
assert.throws(() => { new Console(options); }, mustBeOneOfError);

options.highlight = { warn: { indicator: null } };
assert.throws(() => { new Console(options); }, invalidTypeError);

options.highlight = { error: 'abc' };
assert.throws(() => { new Console(options); }, invalidTypeError);

options.highlight = { error: { bgColor: 'abc' } };
assert.throws(() => { new Console(options); }, mustBeOneOfError);

options.highlight = { error: { indicator: null } };
assert.throws(() => { new Console(options); }, invalidTypeError);

options.highlight = {
  warn: { bgColor: 'bgYellow', indicator: '[W]' },
  error: { bgColor: 'bgRed', indicator: '[E]' },
};

const newInstance = new Console(options);
newInstance.warn(new Error('test'));
newInstance.error(new Error('test'));
