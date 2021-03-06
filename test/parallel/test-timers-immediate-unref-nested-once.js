'use strict';

const common = require('../common');

// This immediate should not execute as it was unrefed
// and nothing else is keeping the event loop alive
setImmediate(() => {
  setImmediate(common.mustNotCall()).unref();
});
