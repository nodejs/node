// This tests --build-sea when the "main" field is missing or invalid.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Empty config (missing main and output)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('empty.json');
  writeFileSync(config, '{}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"main" field of .*empty\.json is not a non-empty string/,
    });
}

// Test: Missing "main" field
{
  tmpdir.refresh();
  const config = tmpdir.resolve('no-main.json');
  writeFileSync(config, '{"output": "sea"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"main" field of .*no-main\.json is not a non-empty string/,
    });
}

// Test: "main" field is empty string
{
  tmpdir.refresh();
  const config = tmpdir.resolve('empty-main.json');
  writeFileSync(config, '{"main": "", "output": "sea"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"main" field of .*empty-main\.json is not a non-empty string/,
    });
}

// Test: Non-existent main script (relative path)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('nonexistent-main-relative.json');
  writeFileSync(config, '{"main": "bundle.js", "output": "sea"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read main script .*bundle\.js/,
    });
}

// Test: Non-existent main script (absolute path)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('nonexistent-main-absolute.json');
  const main = tmpdir.resolve('bundle.js');
  const configJson = JSON.stringify({
    main,
    output: 'sea',
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read main script .*bundle\.js/,
    });
}
