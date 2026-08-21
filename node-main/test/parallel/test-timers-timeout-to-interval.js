'use strict';
const common = require('../common');

// This isn't officially supported but nonetheless is something that is
// currently possible and as such it shouldn't cause the process to crash

const t = setTimeout(common.mustCall(() => {
  if (t._repeat) {
    clearInterval(t);
  }
  t._repeat = 1;
}, 2), 1);
