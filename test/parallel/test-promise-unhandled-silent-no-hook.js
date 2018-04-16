// Flags: --expose-gc
'use strict';

const common = require('../common');

process.env.NODE_UNHANDLED_REJECTION = 'SILENT';

// Verify that ignoring unhandled rejection works fine and that no warning is
// logged even though there is no unhandledRejection hook attached.

new Promise(() => {
  throw new Error('One');
});

Promise.reject('test');

global.gc();

process.on('warning', common.mustNotCall('warning'));
process.on('uncaughtException', common.mustNotCall('uncaughtException'));

setTimeout(common.mustCall(), 2);
