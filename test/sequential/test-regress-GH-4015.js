'use strict';
require('../common');
const { fixturesDir } = require('../common/fixtures');
const assert = require('assert');
const { exec } = require('child_process');

const cmd =
  `"${process.execPath}" "${fixturesDir}/test-regress-GH-4015.js"`;

exec(cmd, function(err, stdout, stderr) {
  assert(/RangeError: Maximum call stack size exceeded/.test(stderr));
});
