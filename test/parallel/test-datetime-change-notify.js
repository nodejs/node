'use strict';

const common = require('../common');

if (!common.hasIntl)
  common.skip('Intl not present.');

const assert = require('assert');

process.env.TZ = 'Etc/UTC';
assert.match(new Date().toString(), /GMT\+0000/);

process.env.TZ = 'America/New_York';
assert.match(new Date().toString(), /Eastern (Standard|Daylight) Time/);

process.env.TZ = 'America/Los_Angeles';
assert.match(new Date().toString(), /Pacific (Standard|Daylight) Time/);

process.env.TZ = 'Europe/Dublin';
assert.match(new Date().toString(), /Irish/);
