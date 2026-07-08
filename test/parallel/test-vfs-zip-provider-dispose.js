// Flags: --experimental-vfs
'use strict';

// Exercises ZipProvider's Symbol.dispose / Symbol.asyncDispose aliases:
// disposing a provider closes its backing archive (a ZipFile releases its
// file descriptor; a ZipBuffer has nothing to close, so disposal is a no-op),
// and the aliases work with the `using` / `await using` declarations.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');
const fsPromises = require('fs/promises');
const zlib = require('zlib');
const vfs = require('node:vfs');

tmpdir.refresh();

async function buildArchive(entries) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

async function writeArchive(name) {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const filePath = path.join(tmpdir.path, name);
  await fsPromises.writeFile(filePath, archive);
  return filePath;
}

(async () => {
  // Symbol.asyncDispose closes a ZipFile-backed provider's archive.
  {
    const zip = await zlib.ZipFile.open(await writeArchive('dispose-async.zip'));
    let closed = 0;
    const original = zip.close.bind(zip);
    zip.close = async (...args) => { closed++; return original(...args); };

    const provider = new vfs.ZipProvider(zip);
    await provider[Symbol.asyncDispose]();
    assert.strictEqual(closed, 1);
  }

  // Symbol.dispose closes a ZipFile-backed provider's archive synchronously.
  {
    const zip = zlib.ZipFile.openSync(await writeArchive('dispose-sync.zip'));
    let closed = 0;
    const original = zip.closeSync.bind(zip);
    zip.closeSync = (...args) => { closed++; return original(...args); };

    const provider = new vfs.ZipProvider(zip);
    provider[Symbol.dispose]();
    assert.strictEqual(closed, 1);
  }

  // `await using` disposes the provider (and closes the archive) at block exit.
  {
    const zip = await zlib.ZipFile.open(await writeArchive('dispose-await-using.zip'));
    let closed = 0;
    const original = zip.close.bind(zip);
    zip.close = async (...args) => { closed++; return original(...args); };

    {
      await using provider = new vfs.ZipProvider(zip);
      assert.strictEqual(provider.readonly, true);
      assert.strictEqual(closed, 0); // Not yet disposed.
    }
    assert.strictEqual(closed, 1); // Disposed at block exit.
  }

  // `using` disposes the provider synchronously at block exit.
  {
    const zip = zlib.ZipFile.openSync(await writeArchive('dispose-using.zip'));
    let closed = 0;
    const original = zip.closeSync.bind(zip);
    zip.closeSync = (...args) => { closed++; return original(...args); };

    {
      using provider = new vfs.ZipProvider(zip);
      assert.strictEqual(provider.readonly, true);
    }
    assert.strictEqual(closed, 1);
  }

  // A ZipBuffer has nothing to close: both aliases are safe no-ops.
  {
    const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
    const provider = new vfs.ZipProvider(new zlib.ZipBuffer(archive));
    provider[Symbol.dispose](); // Does not throw.
    await provider[Symbol.asyncDispose](); // Does not throw.
  }
})().then(common.mustCall());
