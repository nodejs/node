// Flags: --experimental-permission --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();
const assert = require('assert');

// This test ensures that the experimental message is emitted
// when using permission system

process.on('warning', common.mustCall((warning) => {
  assert.match(warning.message, /Permission is an experimental feature/);
}, 1));
