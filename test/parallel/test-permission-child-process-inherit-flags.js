// Flags: --permission --allow-child-process --allow-fs-read=* --allow-worker
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}
if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const assert = require('assert');
const childProcess = require('child_process');

{
  assert.ok(process.permission.has('child'));
}

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}

{
  const { status, stdout } = childProcess.spawnSync(process.execPath,
                                                    [
                                                      '-e',
                                                      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      console.log(process.permission.has("net"));
      console.log(process.permission.has("worker"));
      `,
                                                    ]
  );
  const [fsWrite, fsRead, child, net, worker] = stdout.toString().split('\n');
  assert.strictEqual(status, 0);
  assert.strictEqual(fsWrite, 'false');
  assert.strictEqual(fsRead, 'true');
  assert.strictEqual(child, 'true');
  assert.strictEqual(net, 'false');
  assert.strictEqual(worker, 'true');
}

// It should not override when --permission is passed
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-write=*',
      '-e',
      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      console.log(process.permission.has("net"));
      console.log(process.permission.has("worker"));
      `,
    ]
  );
  const [fsWrite, fsRead, child, net, worker] = stdout.toString().split('\n');
  assert.strictEqual(status, 0);
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
  assert.strictEqual(child, 'false');
  assert.strictEqual(net, 'false');
  assert.strictEqual(worker, 'false');
}

// It should not override when NODE_OPTIONS with --permission is passed
{
  const { status, stdout } = childProcess.spawnSync(
    process.execPath,
    [
      '-e',
      `
      console.log(process.permission.has("fs.write"));
      console.log(process.permission.has("fs.read"));
      console.log(process.permission.has("child"));
      console.log(process.permission.has("net"));
      console.log(process.permission.has("worker"));
      `,
    ],
    {
      env: {
        ...process.env,
        'NODE_OPTIONS': '--permission --allow-fs-write=*',
      }
    }
  );
  const [fsWrite, fsRead, child, net, worker] = stdout.toString().split('\n');
  assert.strictEqual(status, 0);
  assert.strictEqual(fsWrite, 'true');
  assert.strictEqual(fsRead, 'false');
  assert.strictEqual(child, 'false');
  assert.strictEqual(net, 'false');
  assert.strictEqual(worker, 'false');
}

{
  const tmpdir = require('../common/tmpdir');
  const dir = tmpdir.resolve('inherit flags dir');
  const file = `${dir}/file.txt`;

  const grandchildSource = `
    console.log(process.permission.has('fs.read'));
    console.log(process.permission.has('fs.read', ${JSON.stringify(file)}));
    console.log(process.permission.has('fs.read', ${JSON.stringify(__filename)}));
  `;
  const childSource = `
    const { status, stdout, stderr } = require('child_process').spawnSync(
      process.execPath, ['-e', ${JSON.stringify(grandchildSource)}]);
    console.log(JSON.stringify({
      status,
      stderr: stderr.toString(),
      permissions: stdout.toString().trim().split('\\n'),
    }));
  `;

  function run(args) {
    const { status, stdout, stderr } = childProcess.spawnSync(
      process.execPath,
      ['--permission', '--allow-child-process', ...args, '-e', childSource],
    );
    assert.strictEqual(status, 0, stderr.toString());
    const grandchild = JSON.parse(stdout.toString());
    assert.strictEqual(grandchild.status, 0, grandchild.stderr);
    return grandchild.permissions;
  }

  assert.deepStrictEqual(run(['--allow-fs-read', '*']),
                         ['true', 'true', 'true']);

  assert.deepStrictEqual(run(['--allow-fs-read', `${dir}/*`]),
                         ['false', 'true', 'false']);

  assert.deepStrictEqual(run([`--allow-fs-read=${dir}/*`]),
                         ['false', 'true', 'false']);

  assert.deepStrictEqual(
    run(['--allow-fs-read', `${dir}/*`, `--allow-fs-read=${__filename}`]),
    ['false', 'true', 'true']);
}

{
  assert.strictEqual(process.env.NODE_OPTIONS, undefined);
}
