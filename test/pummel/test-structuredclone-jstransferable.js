// Flags: --max-old-space-size=10
'use strict';
require('../common');
const { createHistogram } = require('perf_hooks');

for (let i = 0; i < 1e4; i++) {
  structuredClone(createHistogram());
}
