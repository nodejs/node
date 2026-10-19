'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const app = path.join(tmpdir.path, 'app');
const appx = path.join(tmpdir.path, 'appx');
const denied = path.join(tmpdir.path, 'ap');
fs.mkdirSync(app);
fs.mkdirSync(appx);
fs.writeFileSync(path.join(app, 'file.txt'), 'hello');

const script = `
  const assert = require('assert');
  const fs = require('fs');
  const path = require('path');
  const app = ${JSON.stringify(app)};
  const appx = ${JSON.stringify(appx)};
  const denied = ${JSON.stringify(denied)};
  const file = path.join(app, 'file.txt');

  for (const scope of ['fs.read', 'fs.write']) {
    assert.strictEqual(process.permission.has(scope, app), true);
    assert.strictEqual(process.permission.has(scope, appx), true);
    assert.strictEqual(process.permission.has(scope, file), true);
    assert.strictEqual(process.permission.has(scope, denied), false);
  }

  assert.strictEqual(fs.existsSync(app), true);
  assert.strictEqual(fs.statSync(app).isDirectory(), true);
  assert.ok(fs.readdirSync(app).includes('file.txt'));

  fs.writeFileSync(file, 'updated');
  assert.strictEqual(fs.readFileSync(file, 'utf8'), 'updated');
`;

// Granting two sibling directories used to split the radix tree node holding
// the first directory's `/*` grant, which denied access to the directory
// itself. Check both insertion orders.
for (const grants of [[app, appx], [appx, app]]) {
  const { status, stderr } = spawnSync(process.execPath, [
    '--permission',
    ...grants.map((grant) => `--allow-fs-read=${grant}`),
    ...grants.map((grant) => `--allow-fs-write=${grant}`),
    '-e',
    script,
  ]);
  assert.strictEqual(status, 0, stderr.toString());
}
