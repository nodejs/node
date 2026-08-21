'use strict';

const common = require('../common');
if (!common.hasSQLite)
  common.skip('missing SQLite');

const runBenchmark = require('../common/benchmark');

runBenchmark('sqlite');
