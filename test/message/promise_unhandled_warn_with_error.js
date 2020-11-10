// Flags: --unhandled-rejections=warn-with-error-code
'use strict';

const common = require('../common');
const assert = require('assert');

common.disableCrashOnUnhandledRejection();

Promise.reject(new Error('alas'));
process.on('exit', assert.strictEqual.bind(null, 1));
