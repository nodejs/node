'use strict';

const { spawnSync } = require('child_process');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  process.exit(0);
}

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const dirA = path.join(tmpdir.path, 'dir-a');
const dirB = path.join(tmpdir.path, 'dir-b');
fs.mkdirSync(dirA);
fs.mkdirSync(dirB);
fs.writeFileSync(path.join(dirA, 'a.txt'), 'aaa');
fs.writeFileSync(path.join(dirB, 'b.txt'), 'bbb');

const child = spawnSync(process.execPath, [
  '--permission',
  `--allow-fs-read=${dirA}`,
  `--allow-fs-read=${dirB}`,
  '-e',
  `
    const assert = require('assert');
    const fs = require('fs');
    const path = require('path');

    const dirA = ${JSON.stringify(dirA)};
    const dirB = ${JSON.stringify(dirB)};

    assert.ok(process.permission.has('fs.read', path.join(dirA, 'a.txt')));
    assert.ok(process.permission.has('fs.read', path.join(dirB, 'b.txt')));

    process.permission.drop('fs.read', dirA);

    assert.ok(!process.permission.has('fs.read', path.join(dirA, 'a.txt')));
    assert.throws(() => {
      fs.readFileSync(path.join(dirA, 'a.txt'));
    }, { code: 'ERR_ACCESS_DENIED' });

    assert.ok(process.permission.has('fs.read', path.join(dirB, 'b.txt')));
    assert.strictEqual(
      fs.readFileSync(path.join(dirB, 'b.txt'), 'utf8'),
      'bbb'
    );
  `,
]);

if (child.status !== 0) {
  console.error('stdout:', child.stdout?.toString());
  console.error('stderr:', child.stderr?.toString());
}
assert.strictEqual(child.status, 0, 'Child process should exit with code 0');
