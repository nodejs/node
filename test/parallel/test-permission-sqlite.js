// Flags: --permission --allow-fs-read=* --allow-fs-write=* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfSQLiteMissing();

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fixtures = require('../common/fixtures');

const spawnOpts = {
  encoding: 'utf8',
};

const sqliteCode = {
  // Open database in read-only mode (should require fs.read)
  readOnly: `const sqlite = require('node:sqlite');
    const db = new sqlite.DatabaseSync(process.env.DB_PATH, { readOnly: true });
    db.close();`,

  // Open database in read-write mode (should require fs.write)
  readWrite: `const sqlite = require('node:sqlite');
    const db = new sqlite.DatabaseSync(process.env.DB_PATH);
    db.close();`,

  // Open database with options.open = false (no FS access expected at construction)
  noOpen: `const sqlite = require('node:sqlite');
    const db = new sqlite.DatabaseSync(process.env.DB_PATH, { open: false });
    db.open();
    db.close();`,
};

const dbPath = fixtures.path('permission', 'sqlite.db');

{
  // Opening a database read-only should be blocked when fs.read is not granted
  const code = sqliteCode.readOnly.replace(/\n/g, ' ');
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=/nonexistent',
      '--eval', code,
    ],
    {
      ...spawnOpts,
      env: {
        ...process.env,
        DB_PATH: dbPath,
      },
    },
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  // Opening a database read-write should be blocked when fs.write is not granted
  const code = sqliteCode.readWrite.replace(/\n/g, ' ');
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-write=/nonexistent',
      '--eval', code,
    ],
    {
      ...spawnOpts,
      env: {
        ...process.env,
        DB_PATH: dbPath,
      },
    },
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  // Opening a database read-write should be allowed when permissions are granted
  const code = sqliteCode.readWrite.replace(/\n/g, ' ');
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-write=*',
      '--allow-fs-read=*',
      '--eval', code,
    ],
    {
      ...spawnOpts,
      env: {
        ...process.env,
        DB_PATH: dbPath,
      },
    },
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}

{
  // Opening with open: false should not check permissions at construction,
  // but db.open() should still check permissions
  const code = sqliteCode.noOpen.replace(/\n/g, ' ');
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=/nonexistent',
      '--allow-fs-write=/nonexistent',
      '--eval', code,
    ],
    {
      ...spawnOpts,
      env: {
        ...process.env,
        DB_PATH: dbPath,
      },
    },
  );
  assert.strictEqual(status, 1, `stderr: ${stderr}`);
  assert.match(stderr, /Access to this API has been restricted/);
}

{
  // Memory database should not be restricted
  const code = `const sqlite = require('node:sqlite');
    const db = new sqlite.DatabaseSync(':memory:');
    db.close();`.replace(/\n/g, ' ');
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=/nonexistent',
      '--eval', code,
    ],
    spawnOpts,
  );
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
}
