'use strict';

// A file-backed Blob must keep reading the file it was opened against after
// process.chdir(). a/x and b/x share a size and a full mtime, so the stat
// guard cannot tell them apart: a cwd-relative reopen reads the wrong file.
// Paths are not lexically normalized, so ".." through a symlink must keep
// selecting the same file fs.readFileSync() selected at creation time.
// Refs: https://github.com/nodejs/node/issues/67282

const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

if (process.argv[2] !== 'child') {
  const child = spawnSync(process.execPath, [__filename, 'child'], {
    encoding: 'utf8',
  });
  const output = `${child.stdout ?? ''}${child.stderr ?? ''}`;
  assert.strictEqual(child.status, 0, output);
} else {
  (async () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'openasblob-chdir-'));
    const originalCwd = process.cwd();
    try {
      const dirA = path.join(root, 'a');
      const dirB = path.join(root, 'b');
      const fileA = path.join(dirA, 'x');
      const fileB = path.join(dirB, 'x');
      fs.mkdirSync(dirA);
      fs.mkdirSync(dirB);
      fs.writeFileSync(fileA, 'from-a');
      fs.writeFileSync(fileB, 'from-b');
      const stamp = 1_700_000_000;
      fs.utimesSync(fileA, stamp, stamp);
      fs.utimesSync(fileB, stamp, stamp);
      const statA = fs.statSync(fileA, { bigint: true });
      const statB = fs.statSync(fileB, { bigint: true });
      assert.strictEqual(statA.size, statB.size);
      assert.strictEqual(statA.mtimeNs, statB.mtimeNs);

      const symDir = path.join(root, 'sym');
      let symlinkCase = null;
      if (common.canCreateSymLink()) {
        fs.mkdirSync(path.join(symDir, 'target', 'sub'), { recursive: true });
        fs.writeFileSync(path.join(symDir, 'target', 'file'), 'via-symlink');
        fs.writeFileSync(path.join(symDir, 'file'), 'via-lexical');
        fs.symlinkSync(path.join('target', 'sub'), path.join(symDir, 'link'), 'dir');
        symlinkCase = true;
      }

      const blobs = [];
      const track = async (open, expected) => {
        blobs.push({ blob: await open(), expected });
      };
      async function flush() {
        const pending = blobs.splice(0, blobs.length);
        for (const item of pending) {
          assert.strictEqual(await item.blob.text(), item.expected);
          assert.strictEqual(await item.blob.text(), item.expected);
          const head = item.blob.slice(0, 4);
          assert.strictEqual(await head.text(), item.expected.slice(0, 4));
          assert.strictEqual(
            await item.blob.slice(4).text(),
            item.expected.slice(4),
          );
          assert.strictEqual(await head.slice(1, 3).text(), item.expected.slice(1, 3));
        }
      }

      process.chdir(dirA);
      // POSIX reports the path the caller passed. Windows reports the
      // namespaced path with the long-path prefix removed, which is the
      // resolved absolute path.
      const missing = 'no-such-blob-file';
      const missingPath = common.isWindows ? path.resolve(missing) : missing;
      await assert.rejects(async () => fs.openAsBlob(missing), {
        code: 'ENOENT',
        syscall: 'stat',
        path: missingPath,
      });
      assert.throws(() => fs.openAsBlobSync(missing), {
        code: 'ENOENT',
        syscall: 'stat',
        path: missingPath,
      });

      await track(() => fs.openAsBlob('x'), 'from-a');
      await track(() => fs.openAsBlob(Buffer.from('x')), 'from-a');
      await track(() => fs.openAsBlobSync('x'), 'from-a');
      await track(() => fs.openAsBlobSync(Buffer.from('x')), 'from-a');
      await track(() => fs.openAsBlob(fileA), 'from-a');
      await track(() => fs.openAsBlobSync(Buffer.from(fileA)), 'from-a');
      await track(() => fs.openAsBlob(pathToFileURL(fileA)), 'from-a');

      if (common.isWindows) {
        const letter = path.parse(fileA).root[0];
        const driveRelative = `${letter}:x`;
        await track(() => fs.openAsBlob(driveRelative), 'from-a');
        await track(() => fs.openAsBlobSync(driveRelative), 'from-a');
        const namespaced = path.toNamespacedPath(fileA);
        await track(() => fs.openAsBlob(namespaced), 'from-a');
        await track(() => fs.openAsBlobSync(Buffer.from(namespaced)), 'from-a');
      }

      // Read while cwd is b. b/x matches a/x in size and full mtime.
      process.chdir(dirB);
      await flush();

      await track(() => fs.openAsBlob(path.join('..', 'a', 'x')), 'from-a');
      await track(() => fs.openAsBlobSync(path.join('..', 'a', 'x')), 'from-a');
      process.chdir(originalCwd);
      await flush();

      if (symlinkCase) {
        process.chdir(symDir);
        // path.join() would collapse ".." before the symlink is traversed.
        const relativeLink = ['link', '..', 'file'].join(path.sep);
        const atCreation = fs.readFileSync(relativeLink, 'utf8');
        const lexical = fs.readFileSync(path.resolve(relativeLink), 'utf8');
        if (!common.isWindows) {
          assert.notStrictEqual(atCreation, lexical);
        }
        await track(() => fs.openAsBlob(relativeLink), atCreation);
        await track(() => fs.openAsBlobSync(relativeLink), atCreation);
        process.chdir(dirB);
        await flush();
      }

      if (!common.isWindows) {
        const rawName = Buffer.from([0x66, 0xff, 0xfe]);
        const rawA = Buffer.concat([Buffer.from(`${dirA}/`), rawName]);
        const rawB = Buffer.concat([Buffer.from(`${dirB}/`), rawName]);
        fs.writeFileSync(rawA, 'raw-a');
        fs.writeFileSync(rawB, 'raw-b');
        fs.utimesSync(rawA, stamp, stamp);
        fs.utimesSync(rawB, stamp, stamp);
        const rawStatA = fs.statSync(rawA, { bigint: true });
        const rawStatB = fs.statSync(rawB, { bigint: true });
        assert.strictEqual(rawStatA.size, rawStatB.size);
        assert.strictEqual(rawStatA.mtimeNs, rawStatB.mtimeNs);
        process.chdir(dirA);
        await track(() => fs.openAsBlob(rawName), 'raw-a');
        await track(() => fs.openAsBlobSync(rawName), 'raw-a');
        process.chdir(dirB);
        await flush();
      }
    } finally {
      try {
        process.chdir(originalCwd);
      } catch {
        // Removal still runs when the original directory cannot be restored.
      }
      fs.rmSync(root, { recursive: true, force: true });
    }
  })().then(common.mustCall());
}
