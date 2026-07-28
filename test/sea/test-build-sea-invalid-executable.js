// This tests --build-sea when the "executable" field is invalid.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Invalid "executable" type (should be string)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-executable-type.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "executable": 123
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"executable" field of .*invalid-executable-type\.json is not a non-empty string/,
    });
}

// Test: "executable" field is empty string
{
  tmpdir.refresh();
  const config = tmpdir.resolve('empty-executable.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "executable": ""
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"executable" field of .*empty-executable\.json is not a non-empty string/,
    });
}

// Test: Non-existent executable path
{
  tmpdir.refresh();
  const config = tmpdir.resolve('nonexistent-executable.json');
  const main = tmpdir.resolve('bundle.js');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output: 'sea',
    executable: '/nonexistent/path/to/node',
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Couldn't stat executable.*\/nonexistent\/path\/to\/node: no such file or directory/,
    });
}

// Test: Executable is not a valid binary format (text file)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-executable-format.json');
  const main = tmpdir.resolve('bundle.js');
  const invalidExe = tmpdir.resolve('invalid-exe.txt');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  writeFileSync(invalidExe, 'this is not a valid executable', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output: 'sea',
    executable: invalidExe,
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Executable must be a supported format: ELF, PE, or Mach-O/,
    });
}
