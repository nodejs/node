'use strict';

const common = require('../common');

process.on('beforeExit', common.mustCall(() => {
  setTimeout(common.mustNotCall(), 1).unref();
}));
