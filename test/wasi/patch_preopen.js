'use strict';

require('../common');

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

require('internal/wasi').defaultConfig = {
  preopenDirectories: {
    '/sandbox': fixtures.path('wasi/sandbox'),
    '/tmp': tmpdir.path,
  },
  args: process.argv,
  env: process.env,
};
