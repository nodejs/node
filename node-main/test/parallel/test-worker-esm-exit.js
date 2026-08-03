'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { Worker } = require('worker_threads');

const w = new Worker(fixtures.path('es-modules/import-process-exit.mjs'));
w.on('error', common.mustNotCall());
w.on('exit',
     common.mustCall((code) => assert.strictEqual(code, 42))
);
