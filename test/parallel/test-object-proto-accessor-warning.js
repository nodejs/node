'use strict';

const common = require('../common');

process.on('warning', common.mustCall());

let obj = {};
obj.__proto__;
obj.__proto__ = null;
