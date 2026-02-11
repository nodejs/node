'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

spawn(
  process.execPath,
  [
    '--no-warnings',
    '--test-reporter', 'spec',
    '--test-concurrency', '1',
    '--test-random-seed=12345',
    '--test',
    fixtures.path('test-runner/shards/a.cjs'),
    fixtures.path('test-runner/shards/b.cjs'),
    fixtures.path('test-runner/shards/c.cjs'),
    fixtures.path('test-runner/shards/d.cjs'),
    fixtures.path('test-runner/shards/e.cjs'),
    fixtures.path('test-runner/shards/f.cjs'),
    fixtures.path('test-runner/shards/g.cjs'),
    fixtures.path('test-runner/shards/h.cjs'),
    fixtures.path('test-runner/shards/i.cjs'),
    fixtures.path('test-runner/shards/j.cjs'),
  ],
  { stdio: 'inherit' },
);
