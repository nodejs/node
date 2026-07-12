// Flags: --permission --allow-fs-read=* --allow-fs-write=* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfSQLiteMissing();

const { isMainThread } = require('node:worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('node:assert');
const fs = require('node:fs');
const { spawnSync } = require('node:child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dbPath = fixtures.path('permission', 'sqlite.db');
const deniedPath = tmpdir.resolve('denied.sqlite');
const backupPath = tmpdir.resolve('backup.sqlite');
const allowedBackupPath = tmpdir.resolve('allowed-backup.sqlite');
const sidecarDbPath = tmpdir.resolve('sidecars.sqlite');

function run(code, permissions, env = {}) {
  return spawnSync(
    process.execPath,
    [
      '--permission',
      ...permissions,
      '--eval', code.replace(/\n/g, ' '),
    ],
    {
      encoding: 'utf8',
      env: {
        ...process.env,
        DB_PATH: dbPath,
        ...env,
      },
    },
  );
}

const readOnly = `const { DatabaseSync } = require('node:sqlite');
  const db = new DatabaseSync(process.env.DB_PATH, { readOnly: true });
  db.close();`;

const readWrite = `const { DatabaseSync } = require('node:sqlite');
  const db = new DatabaseSync(process.env.DB_PATH);
  db.close();`;

{
  const { status, stderr } = run(
    readOnly,
    ['--allow-fs-read=' + deniedPath],
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  // A read-write database requires both read and write permission.
  const { status, stderr } = run(
    readWrite,
    ['--allow-fs-read=' + dbPath],
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  const { status, stderr } = run(
    readWrite,
    ['--allow-fs-write=' + dbPath],
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  const { status, stderr } = run(
    readWrite,
    [
      '--allow-fs-read=' + dbPath,
      '--allow-fs-write=' + dbPath,
    ],
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // Delayed opening performs the same checks as opening in the constructor.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH, { open: false });
    db.open();`;
  const { status, stderr } = run(
    code,
    [
      '--allow-fs-read=' + deniedPath,
      '--allow-fs-write=' + deniedPath,
    ],
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  // In-memory databases never require filesystem permission.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(':memory:');
    db.close();`;
  const { status, stderr } = run(code, []);
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // SQLite URI filenames and URI access modes remain supported.
  const code = `const { pathToFileURL } = require('node:url');
    const { DatabaseSync } = require('node:sqlite');
    const url = pathToFileURL(process.env.DB_PATH);
    url.searchParams.set('mode', 'ro');
    const db = new DatabaseSync(url);
    db.close();`;
  const { status, stderr } = run(
    code,
    ['--allow-fs-read=' + dbPath],
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync('file:shared?mode=memory&cache=shared');
    db.exec('CREATE TABLE data(value)');
    db.close();`;
  const { status, stderr } = run(code, []);
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // A URI cannot select an unwrapped VFS while permissions are enabled.
  const code = `const { pathToFileURL } = require('node:url');
    const { DatabaseSync } = require('node:sqlite');
    const url = pathToFileURL(process.env.DB_PATH);
    url.search = '?' + process.env.QUERY;
    new DatabaseSync(url);`;
  for (const query of ['%76fs=unix', 'vfs%00ignored=unix']) {
    const { status, stderr } = run(
      code,
      [
        '--allow-fs-read=' + dbPath,
        '--allow-fs-write=' + dbPath,
      ],
      { QUERY: query },
    );
    assert.strictEqual(status, 1, `stderr: ${stderr}`);
    assert.match(stderr, /must not specify a SQLite VFS/);
  }
}

{
  // Attached databases use the permission VFS too.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    db.prepare('ATTACH DATABASE ? AS outside').run(process.env.ATTACH_PATH);`;
  const { status, stderr } = run(
    code,
    [
      '--allow-fs-read=' + dbPath,
      '--allow-fs-write=' + dbPath,
    ],
    { ATTACH_PATH: deniedPath },
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /unable to open database/);
}

{
  // URI VFS overrides on ATTACH cannot bypass the permission VFS, even if
  // the public authorizer callback is cleared.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(':memory:');
    db.setAuthorizer(null);
    db.exec("ATTACH 'file::memory:?%76fs=memdb' AS outside");`;
  const { status, stderr } = run(code, []);
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /not authorized/);
}

{
  // Backup destinations are opened through the permission VFS.
  const code = `(async () => {
    const { backup, DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH, { readOnly: true });
    try {
      await backup(db, process.env.BACKUP_PATH);
      process.exitCode = 1;
    } catch {
      process.exitCode = 0;
    }
  })();`;
  const { status, stderr } = run(
    code,
    ['--allow-fs-read=' + dbPath],
    { BACKUP_PATH: backupPath },
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(fs.existsSync(backupPath), false);
}

{
  // Backup I/O on worker threads remains available when permissions allow it.
  const code = `(async () => {
    const { backup, DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH, { readOnly: true });
    await backup(db, process.env.BACKUP_PATH);
    db.close();
  })();`;
  const { status, stderr } = run(
    code,
    [
      '--allow-fs-read=' + dbPath,
      '--allow-fs-read=' + allowedBackupPath,
      '--allow-fs-write=' + allowedBackupPath,
    ],
    { BACKUP_PATH: allowedBackupPath },
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(fs.existsSync(allowedBackupPath), true);
}

{
  // A database grant also covers SQLite-managed journal, WAL, and SHM files.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    db.exec('PRAGMA journal_mode=WAL; CREATE TABLE data(value)');
    db.close();`;
  const { status, stderr } = run(
    code,
    [
      '--allow-fs-read=' + sidecarDbPath,
      '--allow-fs-write=' + sidecarDbPath,
    ],
    { DB_PATH: sidecarDbPath },
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // Empty filenames create anonymous temporary files. A path-specific grant
  // cannot safely authorize them.
  const code = `const { DatabaseSync } = require('node:sqlite');
    new DatabaseSync('');`;
  const { status, stderr } = run(
    code,
    [
      '--allow-fs-read=' + deniedPath,
      '--allow-fs-write=' + deniedPath,
    ],
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}
