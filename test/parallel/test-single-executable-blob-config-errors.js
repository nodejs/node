// This tests invalid options for --experimental-sea-config.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { writeFileSync, mkdirSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

{
  tmpdir.refresh();
  const config = 'non-existent-relative.json';
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read single executable configuration from non-existent-relative\.json/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('non-existent-absolute.json');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read single executable configuration from .*non-existent-absolute\.json/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid.json');
  writeFileSync(config, '\n{\n"main"', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /INCOMPLETE_ARRAY_OR_OBJECT/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('empty.json');
  writeFileSync(config, '{}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"main" field of .*empty\.json is not a non-empty string/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('no-main.json');
  writeFileSync(config, '{"output": "test.blob"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"main" field of .*no-main\.json is not a non-empty string/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('no-output.json');
  writeFileSync(config, '{"main": "bundle.js"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"output" field of .*no-output\.json is not a non-empty string/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-disableExperimentalSEAWarning.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": "💥"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"disableExperimentalSEAWarning" field of .*invalid-disableExperimentalSEAWarning\.json is not a Boolean/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('nonexistent-main-relative.json');
  writeFileSync(config, '{"main": "bundle.js", "output": "sea.blob"}', 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read main script .*bundle\.js/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('nonexistent-main-absolute.json');
  const main = tmpdir.resolve('bundle.js');
  const configJson = JSON.stringify({
    main,
    output: 'sea.blob'
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read main script .*bundle\.js/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('output-is-dir-absolute.json');
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
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot write output to .*output-dir/
    });
}

{
  tmpdir.refresh();
  const config = tmpdir.resolve('output-is-dir-relative.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output-dir');
  mkdirSync(output);
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output: 'output-dir'
  });
  writeFileSync(config, configJson, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot write output to output-dir/
    });
}
