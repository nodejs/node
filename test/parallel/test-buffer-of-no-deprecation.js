'use strict';

const common = require('../common');

process.on('warning', common.mustNotCall());

Buffer.of(0, 1);
