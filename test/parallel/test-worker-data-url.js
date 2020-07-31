'use strict';

const common = require('../common');
const { Worker } = require('worker_threads');

new Worker(new URL('data:text/javascript,'))
  .on('error', common.mustNotCall(() => {}));
new Worker(new URL('data:text/javascript,export{}'))
  .on('error', common.mustNotCall(() => {}));

new Worker(new URL('data:text/plain,'))
  .on('error', common.mustCall(() => {}));
new Worker(new URL('data:text/javascript,module.exports={}'))
  .on('error', common.mustCall(() => {}));
