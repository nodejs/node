'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!common.hasIntl)
  common.skip('Intl not present.');

if (!isMainThread)
  common.skip('Test not support running within a worker');

const assert = require('assert');

const cases = [
  {
    timeZone: 'Etc/UTC',
    expected: /Coordinated Universal Time/,
  },
  {
    timeZone: 'America/New_York',
    expected: /Eastern (?:Standard|Daylight) Time/,
  },
  {
    timeZone: 'America/Los_Angeles',
    expected: /Pacific (?:Standard|Daylight) Time/,
  },
  {
    timeZone: 'Europe/Dublin',
    expected: /Irish Standard Time|Greenwich Mean Time/,
  },
];

for (const { timeZone, expected } of cases) {
  process.env.TZ = timeZone;
  const date = new Date().toLocaleString('en-US', { timeZoneName: 'long' });
  assert.match(date, expected);
}
