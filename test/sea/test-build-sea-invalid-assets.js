// This tests --build-sea when the "assets" field is invalid.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Invalid "assets" type (should be object)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-assets-type.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "assets": ["a.txt", "b.txt"]
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"assets" field of .*invalid-assets-type\.json is not a map of strings/,
    });
}

// Test: Invalid asset value type (should be string)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-asset-value.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "assets": {"key": 123}
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"assets" field of .*invalid-asset-value\.json is not a map of strings/,
    });
}

// Test: Non-existent asset file
{
  tmpdir.refresh();
  const config = tmpdir.resolve('nonexistent-asset.json');
  const main = tmpdir.resolve('bundle.js');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output: 'sea',
    assets: {
      'missing': 'nonexistent-asset.txt',
    },
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read asset.*nonexistent-asset\.txt/,
    });
}
