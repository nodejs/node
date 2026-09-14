'use strict';

const common = require('../common');

const assert = require('node:assert');
const crypto = common.hasCrypto ? require('node:crypto') : null;
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const {
  cp,
  glob,
  readFile,
  readdir,
  writeFile,
} = require('node:fs/promises');
const { createHistogram } = require('node:perf_hooks');
const { it } = require('node:test');
const zlib = require('node:zlib');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const sourceDir = tmpdir.resolve('source');
const sourceFile = `${sourceDir}/file.js`;
fs.mkdirSync(`${sourceDir}/nested`, { recursive: true });
fs.writeFileSync(sourceFile, 'data');

async function assertPublishes(t, type, trigger) {
  const channel = `threadpool.work.${type}`;
  let published = false;
  const handler = () => { published = true; };
  dc.subscribe(channel, handler);
  t.after(() => dc.unsubscribe(channel, handler));

  await trigger();

  assert.ok(published);
}

it('publishes for crypto work', { skip: !common.hasCrypto }, (t) => {
  return assertPublishes(t, 'crypto', () => new Promise((resolve, reject) => {
    crypto.pbkdf2('password', '0123456789abcdef', 1000, 16, 'sha256',
                  (err) => {
                    if (err) reject(err);
                    else resolve();
                  });
  }));
});

it('publishes for fs.cp work', (t) => {
  return assertPublishes(t, 'fs.cp', () =>
    cp(sourceDir, tmpdir.resolve('copy'), { recursive: true }));
});

it('publishes for fs.readfile work', (t) => {
  return assertPublishes(t, 'fs.readfile', () => readFile(sourceFile));
});

it('publishes for fs.writefile work', (t) => {
  return assertPublishes(t, 'fs.writefile', () =>
    writeFile(tmpdir.resolve('written'), 'data'));
});

it('publishes for glob work', (t) => {
  return assertPublishes(t, 'glob', () =>
    Array.fromAsync(glob('**', { cwd: sourceDir })));
});

it('publishes for histogram.qrde work', (t) => {
  return assertPublishes(t, 'histogram.qrde', () => {
    const histogram = createHistogram();
    histogram.record(1);
    return histogram.qrde({ bins: 1 });
  });
});

it('publishes for readdir_recursive work', (t) => {
  return assertPublishes(t, 'readdir_recursive', () =>
    readdir(sourceDir, { recursive: true }));
});

it('publishes for node_sqlite3.BackupJob work', {
  skip: !common.hasSQLite,
}, (t) => {
  return assertPublishes(t, 'node_sqlite3.BackupJob', async () => {
    const { backup, DatabaseSync } = require('node:sqlite');
    const database = new DatabaseSync(':memory:');
    t.after(() => database.close());
    await backup(database, tmpdir.resolve('backup.db'));
  });
});

it('publishes for zlib work', (t) => {
  return assertPublishes(t, 'zlib', () => new Promise((resolve, reject) => {
    zlib.gzip('data', (err) => {
      if (err) reject(err);
      else resolve();
    });
  }));
});
