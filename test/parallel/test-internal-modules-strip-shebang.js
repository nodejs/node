'use strict';
const common = require('../common');

common.requireFlags(['--expose-internals']);

const assert = require('assert');
const stripShebang = require('internal/modules/cjs/helpers').stripShebang;

[
  ['', ''],
  ['aa', 'aa'],
  ['#!', ''],
  ['#!bin/bash', ''],
  ['#!bin/bash\naa', '\naa'],
].forEach((i) => assert.strictEqual(stripShebang(i[0]), i[1]));
