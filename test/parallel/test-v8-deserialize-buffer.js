'use strict';

const common = require('../common');
const v8 = require('v8');

process.on('warning', common.mustNotCall());
v8.deserialize(v8.serialize(Buffer.alloc(0)));
