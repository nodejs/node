// Flags: --permission --allow-fs-read=* --allow-child-process
'use strict';

// Regression test for https://github.com/nodejs/node/issues/66901
//
// Two --allow-fs-read grants that are identical up to and including their
// first `*` are deduplicated by GrantAccess, so the colliding insert never
// happens while the flags are applied. permission.drop() calls RebuildTree,
// which replays the recorded grant list unconditionally, and the second
// insert reached an unconditional CHECK in Node::CreateChild and aborted
// the process with SIGABRT.

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dir = path.join(tmpdir.path, 'sig');
const script = path.join(dir, 'app.js');

require('fs').mkdirSync(dir, { recursive: true });
require('fs').writeFileSync(
  script,
  'process.permission.drop("fs.read", process.argv[1]);\n' +
  'console.log("survived");\n',
);

{
  // Colliding grants plus a drop() must not abort the process.
  const { status, signal, stdout, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      `--allow-fs-read=${path.join(dir, '*')}`,
      `--allow-fs-read=${path.join(dir, '*.js')}`,
      script,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(signal, null, `killed by ${signal}\n${stderr}`);
  assert.strictEqual(status, 0, `exited ${status}\n${stderr}`);
  assert.match(stdout, /survived/);
}

{
  // Control: a single grant with the same drop() already worked, and must
  // keep working.
  const { status, signal } = spawnSync(
    process.execPath,
    ['--permission', `--allow-fs-read=${path.join(dir, '*')}`, script],
    { encoding: 'utf8' },
  );

  assert.strictEqual(signal, null);
  assert.strictEqual(status, 0);
}

{
  // The colliding grants must still grant what they say they grant.
  const probe = path.join(dir, 'probe.txt');
  require('fs').writeFileSync(probe, 'ok');

  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      `--allow-fs-read=${path.join(dir, '*')}`,
      `--allow-fs-read=${path.join(dir, '*.js')}`,
      '-e',
      `console.log(require('fs').readFileSync(${JSON.stringify(probe)}, 'utf8'));`,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(status, 0, stderr);
  assert.match(stdout, /ok/);
}
