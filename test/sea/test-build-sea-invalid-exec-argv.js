// This tests --build-sea when the "execArgv" or "execArgvExtension" fields are invalid.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Invalid "execArgv" type (should be array)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-execArgv-type.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "execArgv": "--no-warnings"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"execArgv" field of .*invalid-execArgv-type\.json is not an array of strings/,
    });
}

// Test: Invalid execArgv element type (should be string)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-execArgv-element.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "execArgv": ["--no-warnings", 123]
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"execArgv" field of .*invalid-execArgv-element\.json is not an array of strings/,
    });
}

// Test: Invalid "execArgvExtension" type (should be string)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-execArgvExtension-type.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "execArgvExtension": true
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"execArgvExtension" field of .*invalid-execArgvExtension-type\.json is not a string/,
    });
}

// Test: Invalid "execArgvExtension" value (must be "none", "env", or "cli")
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-execArgvExtension-value.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "execArgvExtension": "invalid"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"execArgvExtension" field of .*invalid-execArgvExtension-value\.json must be one of "none", "env", or "cli"/,
    });
}
