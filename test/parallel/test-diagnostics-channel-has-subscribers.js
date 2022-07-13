'use strict';
require('../common');
const assert = require('assert');
const { channel, hasSubscribers } = require('diagnostics_channel');

const dc = channel('test');
assert.ok(!hasSubscribers('test'));

dc.subscribe(() => {});
assert.ok(hasSubscribers('test'));
