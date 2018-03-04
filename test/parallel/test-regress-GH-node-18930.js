/* eslint-disable node-core/required-modules */
// The ordinarily required common module has side effects on RegExp.
'use strict';

const assert = require('assert');

for (const prop of "&'+123456789_`".split('').map((c) => `$${c}`))
  assert.strictEqual(RegExp[prop], '');
