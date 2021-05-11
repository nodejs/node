'use strict';

require('../common');
const assert = require('assert');

const {
  notifyDateTimeConfigurationChange,
} = require('v8');

process.env.TZ = 'UTC';
notifyDateTimeConfigurationChange();
assert.match(new Date().toString(), /GMT\+0000/);

process.env.TZ = 'EST';
notifyDateTimeConfigurationChange();
assert.match(new Date().toString(), /GMT-05:00/);
