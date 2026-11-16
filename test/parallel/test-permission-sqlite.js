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
const { DatabaseSync } = require('node:sqlite');
const { spawnSyncAndAssert } = require('../common/child_process');
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

const accessDenied = {
  status: 1,
  stderr: /Access to this API has been restricted/,
};

function readWriteGrant(path) {
  return ['--allow-fs-read=' + path, '--allow-fs-write=' + path];
}

function run(code, permissions, expected = {}, env = {}) {
  spawnSyncAndAssert(
    process.execPath,
    [
      '--permission',
      '--allow-env=DB_PATH,ATTACH_PATH,BACKUP_PATH',
      ...permissions,
      '--eval', code.replace(/\n/g, ' '),
    ],
    {
      env: {
        ...process.env,
        DB_PATH: dbPath,
        ...env,
      },
    },
    expected,
  );
}

const readOnly = `const { DatabaseSync } = require('node:sqlite');
  const db = new DatabaseSync(process.env.DB_PATH, { readOnly: true });
  db.close();`;

const readWrite = `const { DatabaseSync } = require('node:sqlite');
  const db = new DatabaseSync(process.env.DB_PATH);
  db.close();`;

run(readOnly, ['--allow-fs-read=' + deniedPath], accessDenied);

// A read-write database requires both read and write permission.
for (const flag of ['--allow-fs-read=', '--allow-fs-write=']) {
  run(readWrite, [flag + dbPath], accessDenied);
}
run(readWrite, readWriteGrant(dbPath));

{
  // Delayed opening performs the same checks as opening in the constructor.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH, { open: false });
    db.open();`;
  run(code, readWriteGrant(deniedPath), accessDenied);
}

{
  // In-memory databases never require filesystem permission.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(':memory:');
    db.close();`;
  run(code, []);
}

{
  // TODO(araujogui): Support URI filenames with the permission model.
  for (const location of [
    'require("node:url").pathToFileURL(process.env.DB_PATH)',
    '"file:" + process.env.DB_PATH + "?mode=ro"',
    'Buffer.from("file::memory:?vfs=memdb")',
  ]) {
    const code = `const { DatabaseSync } = require('node:sqlite');
      new DatabaseSync(${location});`;
    run(code, ['--allow-fs-read=*'], {
      status: 1,
      stderr: /must not be a URL or SQLite URI filename/,
    });
  }
}

{
  // Backup destinations reject URI filenames too.
  const code = `const { backup, DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(':memory:');
    backup(db, 'file:' + process.env.BACKUP_PATH);`;
  run(
    code,
    readWriteGrant('*'),
    { status: 1, stderr: /must not be a URL or SQLite URI filename/ },
    { BACKUP_PATH: backupPath },
  );
  assert.strictEqual(fs.existsSync(backupPath), false);
}

{
  // Attached databases use the permission VFS too. A filename that resembles
  // a sidecar does not inherit permission when opened as a main database.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    const attachPath = process.env.ATTACH_PATH.replaceAll("'", "''");
    db.exec("ATTACH DATABASE '" + attachPath + "' AS outside");`;
  for (const attachPath of [deniedPath, suffixDbPath]) {
    run(
      code,
      readWriteGrant(dbPath),
      { status: 1, stderr: /unable to open database/ },
      { ATTACH_PATH: attachPath },
    );
  }
  assert.strictEqual(fs.existsSync(suffixDbPath), false);
}

{
  // ATTACH treats URI filenames as plain paths, so they cannot select a VFS.
  for (const attach of [
    "db.exec(\"ATTACH 'file::memory:?vfs=memdb' AS outside\");",
    "db.prepare('ATTACH ? AS outside').run('file::memory:?vfs=memdb');",
    "db.exec(\"ATTACH 'file::memory:?vf' || 's=memdb' AS outside\");",
  ]) {
    const code = `const { DatabaseSync } = require('node:sqlite');
      const db = new DatabaseSync(':memory:');
      ${attach}`;
    run(code, [], { status: 1, stderr: /unable to open database/ });
  }
}

{
  // Bound ATTACH filenames work when the path is granted.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(':memory:');
    db.prepare('ATTACH ? AS other').run(process.env.DB_PATH);
    db.close();`;
  run(code, readWriteGrant(dbPath));
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
  run(code, readWriteGrant(linkDir), {}, { DB_PATH: linkedDbPath });
}

{
  // Backup destinations are opened through the permission VFS. Destinations
  // that resemble sidecars still require exact access.
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
  for (const [permissions, destination] of [
    [['--allow-fs-read=' + dbPath], backupPath],
    [readWriteGrant(dbPath), suffixDbPath],
  ]) {
    run(code, permissions, {}, { BACKUP_PATH: destination });
    assert.strictEqual(fs.existsSync(destination), false);
  }
}

{
  // Backup I/O on worker threads remains available when permissions allow it.
  const code = `(async () => {
    const { backup, DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH, { readOnly: true });
    await backup(db, process.env.BACKUP_PATH);
    db.close();
  })();`;
  run(
    code,
    ['--allow-fs-read=' + dbPath, ...readWriteGrant(allowedBackupPath)],
    {},
    { BACKUP_PATH: allowedBackupPath },
  );
  assert.strictEqual(fs.existsSync(allowedBackupPath), true);
}

{
  // A database grant also covers SQLite-managed journal, WAL, and SHM files.
  const code = `const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(process.env.DB_PATH);
    db.exec('PRAGMA journal_mode=WAL; CREATE TABLE data(value)');
    db.close();`;
  run(code, readWriteGrant(sidecarDbPath), {}, { DB_PATH: sidecarDbPath });
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
  try {
    run(code, ['--allow-fs-read=' + walDbPath], {}, { DB_PATH: walDbPath });
  } finally {
    writer.close();
  }
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
  run(code, readWriteGrant(sidecarDbPath), {}, { DB_PATH: sidecarDbPath });
}

{
  // Empty filenames create anonymous temporary databases. A path-specific
  // grant cannot authorize their initial database location.
  const code = `const { DatabaseSync } = require('node:sqlite');
    new DatabaseSync('');`;
  run(code, readWriteGrant(deniedPath), accessDenied);
}
