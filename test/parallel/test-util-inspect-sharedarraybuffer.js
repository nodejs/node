// Flags: --harmony_sharedarraybuffer
/* global SharedArrayBuffer */
'use strict';

require('../common');
const assert = require('assert');
const inspect = require('util').inspect;

assert.strictEqual(inspect(new SharedArrayBuffer(4)),
  'SharedArrayBuffer { byteLength: 4 }');
