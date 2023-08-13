'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const worker = fixtures.path('worker-preload.js');
const { exec } = require('child_process');
const kNodeBinary = process.argv[0];


exec('"$NODE" -r "$WORKER" -pe "1+1"', { env: { NODE: kNodeBinary, WORKER: worker } }, common.mustSucceed());
