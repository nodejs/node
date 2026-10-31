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
const { DatabaseSync } = require('node:sqlite');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dbPath = fixtures.path('permission', 'sqlite.db');
const deniedPath = tmpdir.resolve('denied.sqlite');
const backupPath = tmpdir.resolve('backup.sqlite');
const allowedBackupPath = tmpdir.resolve('allowed-backup.sqlite');
const sidecarDbPath = tmpdir.resolve('sidecars.sqlite');
const walDbPath = tmpdir.resolve('read-only-wal.sqlite');
const suffixDbPath = dbPath + '-wal';

fs.rmSync(suffixDbPath, { force: true });

function run(code, permissions, env = {}) {
  return spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-env=DB_PATH,ATTACH_PATH,BACKUP_PATH,QUERY',
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
  // SQLite processes repeated mode parameters in order.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync('file:unused?mode=ro&mode=memory');
    db.close();`;
  const { status, stderr } = run(code, []);
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // Immutable URI databases are read-only and do not require write access.
  const code = `const { pathToFileURL } = require('node:url');
    const { DatabaseSync } = require('node:sqlite');
    const url = pathToFileURL(process.env.DB_PATH);
    url.searchParams.set('immutable', '1');
    const db = new DatabaseSync(url);
    db.close();`;
  const { status, stderr } = run(
    code,
    ['--allow-fs-read=' + dbPath],
  );
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
  // Attached databases use the permission VFS too. A filename that resembles
  // a sidecar does not inherit permission when opened as a main database.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    const attachPath = process.env.ATTACH_PATH.replaceAll("'", "''");
    db.exec("ATTACH DATABASE '" + attachPath + "' AS outside");`;
  for (const attachPath of [deniedPath, suffixDbPath]) {
    const { status, stderr } = run(
      code,
      [
        '--allow-fs-read=' + dbPath,
        '--allow-fs-write=' + dbPath,
      ],
      { ATTACH_PATH: attachPath },
    );
    assert.strictEqual(status, 1, `stderr: ${stderr}`);
    assert.match(stderr, /unable to open database/);
  }
  assert.strictEqual(fs.existsSync(suffixDbPath), false);
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
  // SQLite does not expose bound or computed ATTACH filenames to the
  // authorizer, so they cannot be checked for a URI VFS and are denied.
  for (const attach of [
    "db.prepare('ATTACH ? AS outside').run('file::memory:?vfs=memdb');",
    "db.exec(\"ATTACH 'file::memory:?vf' || 's=memdb' AS outside\");",
  ]) {
    const code = `const { DatabaseSync } = require('node:sqlite');
      const db = new DatabaseSync(':memory:');
      ${attach}`;
    const { status, stderr } = run(code, []);
    assert.strictEqual(status, 1, `stderr: ${stderr}`);
    assert.match(stderr, /not authorized/);
  }
}

if (!common.isWindows) {
  // Granted paths that go through a symbolic link are checked the same way
  // as the fs module checks them, without resolving the link.
  const realDir = tmpdir.resolve('sqlite-real');
  const linkDir = tmpdir.resolve('sqlite-link');
  fs.mkdirSync(realDir, { recursive: true });
  fs.rmSync(linkDir, { force: true });
  fs.symlinkSync(realDir, linkDir, 'dir');
  const linkedDbPath = tmpdir.resolve('sqlite-link', 'linked.sqlite');
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    db.exec('CREATE TABLE t (x)');
    db.close();`;
  const { status, stderr } = run(
    code,
    [
      '--allow-fs-read=' + linkDir,
      '--allow-fs-write=' + linkDir,
    ],
    { DB_PATH: linkedDbPath },
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
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
  // Backup destinations that resemble sidecars still require exact access.
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
    [
      '--allow-fs-read=' + dbPath,
      '--allow-fs-write=' + dbPath,
    ],
    { BACKUP_PATH: suffixDbPath },
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(fs.existsSync(suffixDbPath), false);
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
  // A read-only connection can consume an existing WAL without write access.
  const writer = new DatabaseSync(walDbPath);
  writer.exec(`
    PRAGMA journal_mode=WAL;
    PRAGMA wal_autocheckpoint=0;
    CREATE TABLE data(value);
    INSERT INTO data VALUES (1);
  `);

  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH, { readOnly: true });
    db.prepare('SELECT * FROM data').all();
    db.close();`;
  const { status, stderr } = run(
    code,
    ['--allow-fs-read=' + walDbPath],
    { DB_PATH: walDbPath },
  );
  writer.close();
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // Internal temporary files fall back to the memory VFS when only
  // path-specific filesystem permissions are available.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    db.exec(\`PRAGMA temp_store=FILE;
      PRAGMA temp.cache_size=1;
      CREATE TEMP TABLE temp_data(value);
      WITH RECURSIVE counter(value) AS (
        VALUES(1) UNION ALL
        SELECT value + 1 FROM counter WHERE value < 1000
      )
      INSERT INTO temp_data SELECT randomblob(10000) FROM counter;\`);
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
  // Empty filenames create anonymous temporary databases. A path-specific
  // grant cannot authorize their initial database location.
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
