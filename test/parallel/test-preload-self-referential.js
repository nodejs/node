'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { exec } = require('child_process');

const nodeBinary = process.argv[0];

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const selfRefModule = fixtures.path('self_ref_module');
const fixtureA = fixtures.path('printA.js');

exec('"$NODE" -r self_ref "$FIXTURE_A"', { cwd: selfRefModule, env: { NODE: nodeBinary, FIXTURE_A: fixtureA } },
     common.mustSucceed((stdout, stderr) => {
       assert.strictEqual(stdout, 'A\n');
     }));
