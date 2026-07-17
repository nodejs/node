// Flags: --unhandled-rejections=none
'use strict';

const common = require('../common');
const assert = require('assert');

// Verify that ignoring unhandled rejection works fine and that no warning is
// logged even though there is no unhandledRejection hook attached.

new Promise(() => {
  throw new Error('One');
});

Promise.reject('test');

process.on('warning', common.mustNotCall('warning'));
process.on('uncaughtException', common.mustNotCall('uncaughtException'));
process.on('exit', assert.strictEqual.bind(null, 0));

setTimeout(common.mustCall(), 2);
