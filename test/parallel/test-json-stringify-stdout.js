'use strict';

// Make sure stdout does not contain circular structures.
// Regression test for https://github.com/nodejs/node/issues/31277

require('../common');

process.stdout.write('');
JSON.stringify(process.stdout);
