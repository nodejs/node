// This tests valid options for --experimental-sea-config.
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { writeFileSync, existsSync } = require('fs');
const { spawnSync } = require('child_process');
const assert = require('assert');

{
  tmpdir.refresh();
  const config = tmpdir.resolve('absolute.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output.blob');
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
  const config = tmpdir.resolve('relative.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output.blob');
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
  const config = tmpdir.resolve('no-disableExperimentalSEAWarning.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output.blob');
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
  const config = tmpdir.resolve('true-disableExperimentalSEAWarning.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output.blob');
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
  const config = tmpdir.resolve('false-disableExperimentalSEAWarning.json');
  const main = tmpdir.resolve('bundle.js');
  const output = tmpdir.resolve('output.blob');
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
