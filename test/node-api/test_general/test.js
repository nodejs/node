'use strict';

const common = require('../../common');
const tmpdir = require('../../common/tmpdir');
const child_process = require('child_process');
const fs = require('fs');
const path = require('path');
const url = require('url');
const filename = require.resolve(`./build/${common.buildType}/test_general`);
const test_general = require(filename);
const assert = require('assert');

tmpdir.refresh();

{
  // TODO(gabrielschulhof): This test may need updating if/when the filename
  // becomes a full-fledged URL.
  assert.strictEqual(test_general.filename, url.pathToFileURL(filename).href);
}

{
  const urlTestDir = path.join(tmpdir.path, 'foo%#bar');
  const urlTestFile = path.join(urlTestDir, path.basename(filename));
  fs.mkdirSync(urlTestDir, { recursive: true });
  fs.copyFileSync(filename, urlTestFile);
  // Use a child process as indirection so that the native module is not loaded
  // into this process and can be removed here.
  const reportedFilename = child_process.spawnSync(
    process.execPath,
    ['-p', `require(${JSON.stringify(urlTestFile)}).filename`],
    { encoding: 'utf8' }).stdout.trim();
  assert.doesNotMatch(reportedFilename, /foo%#bar/);
  assert.strictEqual(reportedFilename, url.pathToFileURL(urlTestFile).href);
  fs.rmSync(urlTestDir, {
    force: true,
    recursive: true,
    maxRetries: 256
  });
}

{
  const [ major, minor, patch, release ] = test_general.testGetNodeVersion();
  assert.strictEqual(process.version.split('-')[0],
                     `v${major}.${minor}.${patch}`);
  assert.strictEqual(release, process.release.name);
}
