'use strict';

// Tests the NODE_REDIRECT_WARNINGS environment variable by spawning
// a new child node process that emits a warning into a temporary
// warnings file. Once the process completes, the warning file is
// opened and the contents are validated

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const fork = require('child_process').fork;
const path = require('path');
const assert = require('assert');

common.refreshTmpDir();

const warnmod = require.resolve(fixtures.path('warnings.js'));
const warnpath = path.join(common.tmpDir, 'warnings.txt');

fork(warnmod, { env: Object.assign({}, process.env,
                                   { NODE_REDIRECT_WARNINGS: warnpath }) })
  .on('exit', common.mustCall(() => {
    fs.readFile(warnpath, 'utf8', common.mustCall((err, data) => {
      assert.ifError(err);
      assert(/\(node:\d+\) Warning: a bad practice warning/.test(data));
    }));
  }));
