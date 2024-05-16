// Flags: --expose_gc
'use strict';
require('../common');
const { Worker } = require('worker_threads');

// Test that it does not crash
new Worker('', { eval: true, env: {} });
new Worker('', { eval: true, env: process.env });
new Worker('', { eval: true, env: { HELLO: 'hi!' } });
