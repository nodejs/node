// This tests invalid options for --experimental-sea-config.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { writeFileSync, mkdirSync } = require('fs');
const { spawnSync } = require('child_process');
const assert = require('assert');
const { join } = require('path');

{
  tmpdir.refresh();
  const config = 'non-existent-relative.json';
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert.match(
    stderr,
    /Cannot read single executable configuration from non-existent-relative\.json/
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'non-existent-absolute.json');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `Cannot read single executable configuration from ${config}`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'invalid.json');
  writeFileSync(config, '\n{\n"main"', 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert.match(stderr, /SyntaxError: Expected ':' after property name/);
  assert(
    stderr.includes(
      `Cannot parse JSON from ${config}`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'empty.json');
  writeFileSync(config, '{}', 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `"main" field of ${config} is not a non-empty string`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'no-main.json');
  writeFileSync(config, '{"output": "test.blob"}', 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `"main" field of ${config} is not a non-empty string`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'no-output.json');
  writeFileSync(config, '{"main": "bundle.js"}', 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `"output" field of ${config} is not a non-empty string`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'invalid-disableExperimentalSEAWarning.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": "💥"
}
  `, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `"disableExperimentalSEAWarning" field of ${config} is not a Boolean`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'nonexistent-main-relative.json');
  writeFileSync(config, '{"main": "bundle.js", "output": "sea.blob"}', 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert.match(stderr, /Cannot read main script bundle\.js/);
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'nonexistent-main-absolute.json');
  const main = join(tmpdir.path, 'bundle.js');
  const configJson = JSON.stringify({
    main,
    output: 'sea.blob'
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });

  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `Cannot read main script ${main}`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'output-is-dir-absolute.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output-dir');
  mkdirSync(output);
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

  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert(
    stderr.includes(
      `Cannot write output to ${output}`
    )
  );
}

{
  tmpdir.refresh();
  const config = join(tmpdir.path, 'output-is-dir-relative.json');
  const main = join(tmpdir.path, 'bundle.js');
  const output = join(tmpdir.path, 'output-dir');
  mkdirSync(output);
  writeFileSync(main, 'console.log("hello")', 'utf-8');
  const configJson = JSON.stringify({
    main,
    output: 'output-dir'
  });
  writeFileSync(config, configJson, 'utf8');
  const child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', config], {
      cwd: tmpdir.path,
    });

  const stderr = child.stderr.toString();
  assert.strictEqual(child.status, 1);
  assert.match(stderr, /Cannot write output to output-dir/);
}
