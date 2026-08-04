'use strict';
require('../common');

for (let i = 0; i < 1e6; i++) {
  performance.mark(`mark-${i}`);
}

performance.getEntriesByName('mark-0');
performance.clearMarks();
