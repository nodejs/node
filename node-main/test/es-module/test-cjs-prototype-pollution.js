'use strict';

const { mustNotCall, mustCall } = require('../common');

Object.defineProperties(Object.prototype, {
  then: {
    set: mustNotCall('set %Object.prototype%.then'),
    get: mustNotCall('get %Object.prototype%.then'),
  },
});

import('data:text/javascript,').then(mustCall());
