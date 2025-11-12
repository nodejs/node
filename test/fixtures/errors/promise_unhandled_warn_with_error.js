// Flags: --unhandled-rejections=warn-with-error-code
'use strict';

require('../../common');
const assert = require('assert');

Promise.reject(new Error('alas'));
process.on('exit', assert.strictEqual.bind(null, 1));
