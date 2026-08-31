// Flags: --expose-internals
'use strict';

// Loading an addon from bytes materializes them into an image in the temporary
// directory, so dlopenBinary() requires file-system write access on top of the
// addon permission every dlopen already needs. Granting only one of the two is
// not enough.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const addonPath = path.join(
  __dirname, '..', 'addons', 'hello-world', 'build', 'Release', 'binding.node');
if (!fs.existsSync(addonPath)) common.skip('the hello-world addon is not built');

// Loads the addon from bytes at a path that does not exist on disk, as an
// addon served by a virtual file system would be, and reports what happened.
const child = `
  const { internalBinding } = require('internal/test/binding');
  const fs = require('fs');
  const os = require('os');
  const path = require('path');
  const { dlopenBinary } = internalBinding('process_methods');
  const bytes = fs.readFileSync(${JSON.stringify(addonPath)});
  const target = path.join(os.tmpdir(), 'nonexistent-vfs-dir', 'binding.node');
  const m = { exports: {} };
  try {
    dlopenBinary(m, target, undefined, bytes);
    console.log('LOADED', m.exports.hello());
  } catch (err) {
    console.log('DENIED', err.code, err.permission ?? '');
  }
`;

function run(...permissions) {
  return spawnSync(
    process.execPath,
    ['--expose-internals', '--permission', '--allow-fs-read=*',
     ...permissions, '-e', child],
    { encoding: 'utf8' });
}

// Both permissions granted: the addon loads.
{
  const res = run('--allow-addons', '--allow-fs-write=*');
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /LOADED world/);
}

// Addons allowed but no write access: refused by the file-system check, so the
// bytes are never materialized.
{
  const res = run('--allow-addons');
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /DENIED ERR_ACCESS_DENIED FileSystemWrite/);
}

// Write access but addons disallowed: refused before the bytes are looked at,
// the same as any other dlopen under the permission model.
{
  const res = run('--allow-fs-write=*');
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /DENIED ERR_DLOPEN_DISABLED/);
}

// Without the permission model neither check applies.
{
  const res = spawnSync(process.execPath, ['--expose-internals', '-e', child],
                        { encoding: 'utf8' });
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /LOADED world/);
}
