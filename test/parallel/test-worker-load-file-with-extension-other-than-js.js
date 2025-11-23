'use strict';
const common = require('../common');
if (!process.config.variables.node_use_amaro) {
  common.skip('Requires Amaro');
}
const fixtures = require('../common/fixtures');
const { Worker } = require('worker_threads');

(common.mustCall(() => {
  new Worker(fixtures.path('worker-script.ts'));
}))();
