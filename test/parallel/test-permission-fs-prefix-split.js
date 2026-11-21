'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const app = path.join(tmpdir.path, 'app');
const appx = path.join(tmpdir.path, 'appx');
const nested = path.join(app, 'nested');
const denied = path.join(tmpdir.path, 'ap');
fs.mkdirSync(app);
fs.mkdirSync(appx);
fs.mkdirSync(nested);
fs.writeFileSync(path.join(app, 'file.txt'), 'hello');
fs.writeFileSync(path.join(nested, 'file.txt'), 'hello');

const script = `
  const assert = require('assert');
  const fs = require('fs');
  const path = require('path');
  const app = ${JSON.stringify(app)};
  const appx = ${JSON.stringify(appx)};
  const nested = ${JSON.stringify(nested)};
  const denied = ${JSON.stringify(denied)};
  const expected = JSON.parse(process.argv[1]);
  const file = path.join(app, 'file.txt');
  const nestedFile = path.join(nested, 'file.txt');

  for (const scope of ['fs.read', 'fs.write']) {
    assert.strictEqual(process.permission.has(scope, app), true);
    assert.strictEqual(process.permission.has(scope, appx), expected.appx);
    assert.strictEqual(process.permission.has(scope, nested), true);
    assert.strictEqual(process.permission.has(scope, file), true);
    assert.strictEqual(process.permission.has(scope, nestedFile), true);
    assert.strictEqual(process.permission.has(scope, denied), false);
  }

  assert.strictEqual(fs.existsSync(app), true);
  assert.strictEqual(fs.statSync(app).isDirectory(), true);
  assert.deepStrictEqual(fs.readdirSync(app).sort(), ['file.txt', 'nested']);

  assert.strictEqual(fs.existsSync(nested), true);
  assert.strictEqual(fs.statSync(nested).isDirectory(), true);
  assert.deepStrictEqual(fs.readdirSync(nested), ['file.txt']);

  fs.writeFileSync(file, 'updated');
  assert.strictEqual(fs.readFileSync(file, 'utf8'), 'updated');
  fs.writeFileSync(nestedFile, 'updated');
  assert.strictEqual(fs.readFileSync(nestedFile, 'utf8'), 'updated');
`;

// Granting directories that share a prefix can split the radix tree node
// holding a `/*` grant, denying access to the directory itself. Cover
// sibling grants in both orders and nested grants where the parent is
// inserted after the child.
const cases = [
  { grants: [app, appx], appx: true },
  { grants: [appx, app], appx: true },
  { grants: [nested, app], appx: false },
  { grants: [app, nested], appx: false },
];

for (const { grants, appx: hasAppx } of cases) {
  const { status, stderr } = spawnSync(process.execPath, [
    '--permission',
    ...grants.map((grant) => `--allow-fs-read=${grant}`),
    ...grants.map((grant) => `--allow-fs-write=${grant}`),
    '-e',
    script,
    JSON.stringify({ appx: hasAppx }),
  ]);
  assert.strictEqual(status, 0, stderr.toString());
}
