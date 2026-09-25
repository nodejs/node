'use strict';

const { add } = require('./probe-multi-a/utils');
const { multiply } = require('./probe-multi-b/utils');

multiply(add(1, 2), 3);
