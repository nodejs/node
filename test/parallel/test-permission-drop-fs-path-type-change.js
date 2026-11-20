'use strict';

require('../common');

const { spawnSync } = require('child_process');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  process.exit(0);
}
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

function runWithGrants(target, script) {
  const child = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${target}`,
    `--allow-fs-write=${target}`,
    '-e',
    `
      const assert = require('assert');
      const fs = require('fs');
      const target = ${JSON.stringify(target)};
      ${script}
    `,
  ]);

  if (child.status !== 0) {
    console.error('stdout:', child.stdout?.toString());
    console.error('stderr:', child.stderr?.toString());
  }
  assert.strictEqual(child.status, 0);
}

// The path does not exist when it is granted and is created as a directory
// before the drop.
{
  const target = path.join(tmpdir.path, 'created-later');

  runWithGrants(target, `
    fs.mkdirSync(target);

    process.permission.drop('fs.read', target);
    assert.ok(!process.permission.has('fs.read', target));
    assert.throws(() => fs.readdirSync(target), { code: 'ERR_ACCESS_DENIED' });
    assert.ok(process.permission.has('fs.write', target));

    process.permission.drop('fs.write', target);
    assert.ok(!process.permission.has('fs.write', target));
    assert.throws(() => fs.rmdirSync(target), { code: 'ERR_ACCESS_DENIED' });
  `);
}

// The path is a directory when it is granted and is replaced by a file
// before the drop.
{
  const target = path.join(tmpdir.path, 'replaced-by-file');
  fs.mkdirSync(target);

  runWithGrants(target, `
    fs.rmdirSync(target);
    fs.writeFileSync(target, 'content');

    process.permission.drop('fs.read', target);
    assert.ok(!process.permission.has('fs.read', target));
    assert.throws(() => fs.readFileSync(target), { code: 'ERR_ACCESS_DENIED' });
    assert.ok(process.permission.has('fs.write', target));

    process.permission.drop('fs.write', target);
    assert.ok(!process.permission.has('fs.write', target));
    assert.throws(() => fs.writeFileSync(target, 'other'), {
      code: 'ERR_ACCESS_DENIED',
    });
  `);
}
