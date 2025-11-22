'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const { Worker } = require('worker_threads');

if (!process.config.variables.node_use_amaro) {
  common.skip('Requires Amaro');
}

(common.mustCall(() => {
  new Worker(fixtures.path('worker-script.ts'));
}))();
