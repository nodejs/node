'use strict';

const fs = require('fs');
const { setTimeout } = require('timers/promises');
const { bench } = require('node:bench');

module.exports = function register(name) {
  const lock = process.env.NODE_BENCH_LOCK;
  fs.writeFileSync(lock, `${process.pid}`, { flag: 'wx' });
  process.on('exit', () => fs.unlinkSync(lock));

  bench(name, { samples: 1 }, async (b) => {
    b.start();
    await setTimeout(25);
    b.end(1);
  });
};
