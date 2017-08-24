'use strict';
const common = require('../common');

process.on('warning', common.fail);
process.on('SIGINT', () => {});
