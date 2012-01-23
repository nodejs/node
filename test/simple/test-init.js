// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.




(function() {
  var assert = require('assert'),
      child = require('child_process'),
      util = require('util'),
      common = require('../common');
  if (process.env['TEST_INIT']) {
    util.print('Loaded successfully!');
  } else {
    // change CWD as we do this test so its not dependant on current CWD
    // being in the test folder
    process.chdir(__dirname);

    child.exec(process.execPath + ' test-init', {env: {'TEST_INIT': 1}},
        function(err, stdout, stderr) {
          assert.equal(stdout, 'Loaded successfully!',
                       '`node test-init` failed!');
        });
    child.exec(process.execPath + ' test-init.js', {env: {'TEST_INIT': 1}},
        function(err, stdout, stderr) {
          assert.equal(stdout, 'Loaded successfully!',
                       '`node test-init.js` failed!');
        });

    // test-init-index is in fixtures dir as requested by ry, so go there
    process.chdir(common.fixturesDir);

    child.exec(process.execPath + ' test-init-index', {env: {'TEST_INIT': 1}},
        function(err, stdout, stderr) {
          assert.equal(stdout, 'Loaded successfully!',
                       '`node test-init-index failed!');
        });

    // ensures that `node fs` does not mistakenly load the native 'fs' module
    // instead of the desired file and that the fs module loads as
    // expected in node
    process.chdir(common.fixturesDir + '/test-init-native/');

    child.exec(process.execPath + ' fs', {env: {'TEST_INIT': 1}},
        function(err, stdout, stderr) {
          assert.equal(stdout, 'fs loaded successfully',
                       '`node fs` failed!');
        });
  }
})();
