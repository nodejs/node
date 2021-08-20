'use strict';

const common = require('../common');

process.on('warning', common.mustCall());

const obj = {};
// eslint-disable-next-line
const _ = obj.__proto__;
// eslint-disable-next-line
obj.__proto__ = null;
