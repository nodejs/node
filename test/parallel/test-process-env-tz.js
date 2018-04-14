'use strict';

const common = require('../common');
const assert = require('assert');

if (common.isWindows)  // Using a different TZ format.
  common.skip('todo: test on Windows');

if (common.isAIX || common.isSunOS)  // Reports 2018 CEST as CET.
  common.skip('tzdata too old');

const date = new Date('2018-04-14T12:34:56.789Z');

process.env.TZ = 'Europe/Amsterdam';
if (/\(Europe\)/.test(date.toString()))
  common.skip('not using bundled ICU');  // Shared library or --with-intl=none.
assert.strictEqual(date.toString(), 'Sat Apr 14 2018 14:34:56 GMT+0200 (CEST)');

process.env.TZ = 'Europe/London';
assert.strictEqual(date.toString(), 'Sat Apr 14 2018 13:34:56 GMT+0100 (BST)');

process.env.TZ = 'Etc/UTC';
assert.strictEqual(date.toString(), 'Sat Apr 14 2018 12:34:56 GMT+0000 (UTC)');
