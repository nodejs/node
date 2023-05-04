// This tests valid options for --experimental-sea-config.
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { writeFileSync, existsSync } = require('fs');
const { spawnSync } = require('child_process');
const assert = require('assert');
const { join } = require('path');

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'absolute.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output.blob');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output,
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  assert.strictEqual(child.status, 0);
  assert(existsSync(output));
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'relative.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output.blob');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main: 'bundle.js',
    output: 'output.blob'
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });

  assert.strictEqual(child.status, 0);
  assert(existsSync(output));
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'no-disableExperimentalSEAWarning.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output.blob');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main: 'bundle.js',
    output: 'output.blob',
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });

  assert.strictEqual(child.status, 0);
  assert(existsSync(output));
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'true-disableExperimentalSEAWarning.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output.blob');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main: 'bundle.js',
    output: 'output.blob',
    disableExperimentalSEAWarning: true,
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });

  assert.strictEqual(child.status, 0);
  assert(existsSync(output));
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'false-disableExperimentalSEAWarning.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output.blob');
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main: 'bundle.js',
    output: 'output.blob',
    disableExperimentalSEAWarning: false,
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });

  assert.strictEqual(child.status, 0);
  assert(existsSync(output));
}
