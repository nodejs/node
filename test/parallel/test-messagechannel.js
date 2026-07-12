'use strict';

const common = require('../common');

// See: https://github.com/nodejs/node/issues/49940
(async () => {
  new MessageChannel().port1.postMessage({}, {
    transfer: {
      *[Symbol.iterator]() {}
    }
  });
})().then(common.mustCall());
