'use strict';

const { setTimeout } = require('timers/promises');
const { bench } = require('node:bench');

const output = Buffer.from('split:\u20ac\n');
process.stdout.write(output.subarray(0, 7));

bench('UTF-8 output', { samples: 1 }, async (b) => {
  b.start();
  await setTimeout(20);
  process.stdout.write(output.subarray(7));
  b.end(1);
});
