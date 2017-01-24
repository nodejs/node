'use strict';
const common = require('../common');
const assert = require('assert');
const child = require('child_process');
const path = require('path');

if (process.env['TEST_INIT']) {
  return process.stdout.write('Loaded successfully!');
}

process.env.TEST_INIT = 1;

function test(file, expected) {
  const path = `"${process.execPath}" ${file}`;
  child.exec(path, {env: process.env}, common.mustCall((err, out) => {
    assert.ifError(err);
    assert.strictEqual(out, expected, `'node ${file}' failed!`);
  }));
}

{
  // change CWD as we do this test so it's not dependent on current CWD
  // being in the test folder
  process.chdir(__dirname);
  test('test-init', 'Loaded successfully!');
  test('test-init.js', 'Loaded successfully!');
}

{
  // test-init-index is in fixtures dir as requested by ry, so go there
  process.chdir(common.fixturesDir);
  test('test-init-index', 'Loaded successfully!');
}

{
  // ensures that `node fs` does not mistakenly load the native 'fs' module
  // instead of the desired file and that the fs module loads as
  // expected in node
  process.chdir(path.join(common.fixturesDir, 'test-init-native'));
  test('fs', 'fs loaded successfully');
}
