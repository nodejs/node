// This tests --build-sea when the "output" field is missing or invalid.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync, mkdirSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Missing "output" field
{
  tmpdir.refresh();
  const config = tmpdir.resolve('no-output.json');
  writeFileSync(config, '{"main": "bundle.js"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"output" field of .*no-output\.json is not a non-empty string/,
    });
}

// Test: "output" field is empty string
{
  tmpdir.refresh();
  const config = tmpdir.resolve('empty-output.json');
  writeFileSync(config, '{"main": "bundle.js", "output": ""}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"output" field of .*empty-output\.json is not a non-empty string/,
    });
}

// Test: Output path is a directory
{
  tmpdir.refresh();
  const config = tmpdir.resolve('output-is-dir.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output-dir');
  mkdirSync(output);
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output,
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Couldn't write output executable.*output-dir/,
    });
}
