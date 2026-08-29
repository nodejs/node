'use strict';

const { bench } = require('node:bench');

bench('blocked run file', { samples: 1 }, async () => {
  await new Promise(() => {});
});
