// Flags: --expose-internals
'use strict';

require('../common');
const { aggregateTwoErrors } = require('internal/errors');

const originalError = new Error('original');
const err = new Error('second error');

originalError.code = 'ERR0';
err.code = 'ERR1';

throw aggregateTwoErrors(err, originalError);
