'use strict';

const common = require('../common');
const assert = require('assert');

const child = require('child_process').spawn(process.execPath, [
  '-r',
  './test/common',
  'test/fixtures/common-detect-global-leak.js'
]);

var stderrOutput = '';

child.on('error', assert.fail);

child.stdout.on('data', assert.fail);

child.stderr.on('data', function(er) {
  stderrOutput += er.toString('utf-8');
});

child.stderr.once('end', common.mustCall(function() {
  assert(/Unknown globals: leak_this_to_global_space/.test(stderrOutput));
}));
