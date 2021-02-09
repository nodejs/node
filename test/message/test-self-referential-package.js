'use strict';

require('../common');
const { spawn } = require('child_process');

const fixtures = require('../common/fixtures');
const selfRefModule = fixtures.path('self_ref_module_with_colon');

spawn(process.argv0, [`${selfRefModule}/bin.js`], {
  stdio: 'inherit'
});
