'use strict';

const common = require('../../common');
const tmpdir = require('../../common/tmpdir');
const fs = require('fs');
const path = require('path');
const filename = require.resolve(`./build/${common.buildType}/test_general`);
const test_general = require(filename);
const assert = require('assert');

tmpdir.refresh();

{
  // TODO(gabrielschulhof): This test may need updating if/when the filename
  // becomes a full-fledged URL.
  assert.strictEqual(
    decodeURIComponent(test_general.filename),
    `file://${filename}`);
}

{
  const urlTestDir = path.join(tmpdir.path, 'foo%?#bar');
  const urlTestFile = path.join(urlTestDir, path.basename(filename));
  fs.mkdirSync(urlTestDir, { recursive: true });
  fs.copyFileSync(filename, urlTestFile);
  const reportedFilename = require(urlTestFile).filename;
  assert.doesNotMatch(reportedFilename, /foo%\?#bar/);
  assert.strictEqual(
    decodeURIComponent(reportedFilename),
    `file://${urlTestFile}`);
}

{
  const [ major, minor, patch, release ] = test_general.testGetNodeVersion();
  assert.strictEqual(process.version.split('-')[0],
                     `v${major}.${minor}.${patch}`);
  assert.strictEqual(release, process.release.name);
}
