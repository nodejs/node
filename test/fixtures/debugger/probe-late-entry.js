'use strict';

import('./probe-late-target.cjs').then(({ add }) => {
  const result = add(3, 2);
  const { multiply } = require('./probe-late-target.mjs');
  console.log(multiply(result, 4));
});
