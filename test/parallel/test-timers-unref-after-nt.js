'use strict';
const common = require('../common');

const timer = setTimeout(() => {
  common.fail('nope');
}, 200);

setImmediate(() => {
  timer.unref();
});

setTimeout(common.fail, 500).unref();
