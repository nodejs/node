'use strict';
const common = require('../common');
const assert = require('assert');
const child = require('child_process');
const util = require('util');
if (process.env['TEST_INIT']) {
  util.print('Loaded successfully!');
} else {
  // change CWD as we do this test so its not dependant on current CWD
  // being in the test folder
  process.chdir(__dirname);

  // slow but simple
  var envCopy = JSON.parse(JSON.stringify(process.env));
  envCopy.TEST_INIT = 1;

  child.exec('"' + process.execPath + '" test-init', {env: envCopy},
      function(err, stdout, stderr) {
        assert.equal(stdout, 'Loaded successfully!',
                     '`node test-init` failed!');
      });
  child.exec('"' + process.execPath + '" test-init.js', {env: envCopy},
      function(err, stdout, stderr) {
        assert.equal(stdout, 'Loaded successfully!',
                     '`node test-init.js` failed!');
      });

  // test-init-index is in fixtures dir as requested by ry, so go there
  process.chdir(common.fixturesDir);

  child.exec('"' + process.execPath + '" test-init-index', {env: envCopy},
      function(err, stdout, stderr) {
        assert.equal(stdout, 'Loaded successfully!',
                     '`node test-init-index failed!');
      });

  // ensures that `node fs` does not mistakenly load the native 'fs' module
  // instead of the desired file and that the fs module loads as
  // expected in node
  process.chdir(common.fixturesDir + '/test-init-native/');

  child.exec('"' + process.execPath + '" fs', {env: envCopy},
      function(err, stdout, stderr) {
        assert.equal(stdout, 'fs loaded successfully',
                     '`node fs` failed!');
      });
}
