'use strict';

// node:vfs is gated behind --experimental-vfs. Without the flag the
// module is not exposed; bare `vfs` (without the node: scheme) is also
// blocked.

require('../common');
const { spawnSync } = require('child_process');
const assert = require('assert');

// Without the flag, requiring node:vfs throws ERR_UNKNOWN_BUILTIN_MODULE.
{
  const r = spawnSync(process.execPath, [
    '-e', 'require("node:vfs")',
  ]);
  assert.strictEqual(r.status, 1);
  assert.match(r.stderr.toString(), /ERR_UNKNOWN_BUILTIN_MODULE/);
}

// Without the flag, importing node:vfs throws ERR_UNKNOWN_BUILTIN_MODULE.
{
  const r = spawnSync(process.execPath, [
    '--input-type=module',
    '-e', 'import("node:vfs").catch((e) => { console.error(e.code); process.exit(1); });',
  ]);
  assert.strictEqual(r.status, 1);
  assert.match(r.stderr.toString(), /ERR_UNKNOWN_BUILTIN_MODULE/);
}

// With the flag, node:vfs loads and works.
{
  const script =
    'const v = require("node:vfs");' +
    'const x = v.create();' +
    'x.writeFileSync("/x", "hi");' +
    'console.log(x.readFileSync("/x", "utf8"));';
  const r = spawnSync(process.execPath, ['--experimental-vfs', '-e', script]);
  assert.strictEqual(r.status, 0, r.stderr.toString());
  assert.strictEqual(r.stdout.toString().trim(), 'hi');
}

// Bare `vfs` (no node: scheme) is always blocked.
{
  const r = spawnSync(process.execPath, [
    '--experimental-vfs',
    '-e', "require('vfs')",
  ]);
  assert.strictEqual(r.status, 1);
  assert.match(r.stderr.toString(), /Cannot find module 'vfs'/);
}
