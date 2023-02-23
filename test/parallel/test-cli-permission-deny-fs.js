'use strict';

require('../common');
const { spawnSync } = require('child_process');
const assert = require('assert');
const fs = require('fs');

{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-permission', '-e',
      `console.log(process.permission.has("fs"));
       console.log(process.permission.has("fs.read"));
       console.log(process.permission.has("fs.write"));`,
    ]
  );

  const [fs, fsIn, fsOut] = stdout.toString().split('\n');
  assert.strictEqual(fs, 'false');
  assert.strictEqual(fsIn, 'false');
  assert.strictEqual(fsOut, 'false');
  assert.strictEqual(status, 0);
}

{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '--allow-fs-write', '/tmp/', '-e',
      `console.log(process.permission.has("fs"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.write", "/tmp/"));`,
    ]
  );
  const [fs, fsIn, fsOut, fsOutAllowed] = stdout.toString().split('\n');
  assert.strictEqual(fs, 'false');
  assert.strictEqual(fsIn, 'false');
  assert.strictEqual(fsOut, 'false');
  assert.strictEqual(fsOutAllowed, 'true');
  assert.strictEqual(status, 0);
}

{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '--allow-fs-write', '*', '-e',
      `console.log(process.permission.has("fs"));
       console.log(process.permission.has("fs.read"));
       console.log(process.permission.has("fs.write"));`,
    ]
  );

  const [fs, fsIn, fsOut] = stdout.toString().split('\n');
  assert.strictEqual(fs, 'false');
  assert.strictEqual(fsIn, 'false');
  assert.strictEqual(fsOut, 'true');
  assert.strictEqual(status, 0);
}

{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '--allow-fs-read', '*', '-e',
      `console.log(process.permission.has("fs"));
       console.log(process.permission.has("fs.read"));
       console.log(process.permission.has("fs.write"));`,
    ]
  );

  const [fs, fsIn, fsOut] = stdout.toString().split('\n');
  assert.strictEqual(fs, 'false');
  assert.strictEqual(fsIn, 'true');
  assert.strictEqual(fsOut, 'false');
  assert.strictEqual(status, 0);
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '--allow-fs-write=*', '-p',
      'fs.readFileSync(process.execPath)',
    ]
  );
  assert.ok(
    stderr.toString().includes('Access to this API has been restricted'),
    stderr);
  assert.strictEqual(status, 1);
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '-p',
      'fs.readFileSync(process.execPath)',
    ]
  );
  assert.ok(
    stderr.toString().includes('Access to this API has been restricted'),
    stderr);
  assert.strictEqual(status, 1);
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '--allow-fs-read=*', '-p',
      'fs.writeFileSync("policy-deny-example.md", "# test")',
    ]
  );
  assert.ok(
    stderr.toString().includes('Access to this API has been restricted'),
    stderr);
  assert.strictEqual(status, 1);
  assert.ok(!fs.existsSync('permission-deny-example.md'));
}
