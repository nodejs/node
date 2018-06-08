// Flags: --pending-deprecation --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

process.on('warning', common.mustNotCall());
v8.deserialize(v8.serialize(Buffer.alloc(0)));
