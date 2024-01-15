'use strict';

const common = require('../common');
const { EventEmitter } = require('events');

setImmediate(async () => {
  const e = await new Promise((resolve) => {
    const e = new EventEmitter();
    resolve(e);
    process.deferTick(common.mustCall(() => {
      e.emit('error', new Error('kaboom'));
    }));
  });
  e.on('error', common.mustCall());
});
