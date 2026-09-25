'use strict';

require('../common');
const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

tmpdir.refresh();

function hasTool(command, args = ['--help']) {
  try {
    const result = spawnSync(command, args, { stdio: 'ignore' });
    // Require a clean exit, not merely a successful spawn: Windows ships an
    // App Execution Alias stub for `python3` that runs, prints "Python was
    // not found", and exits non-zero (9009) - `error` is undefined for it,
    // so a spawn-only check would wrongly report the tool as present.
    return result.error === undefined && result.status === 0;
  } catch {
    return false;
  }
}

// Rendering a UTF-8 entry name (general-purpose bit 11, which we set) through
// Info-ZIP unzip/zipinfo needs two things that are independent of whether the
// archive is well-formed: (1) the build must be compiled with UNICODE_SUPPORT
// - macOS's bundled Info-ZIP is not, and mangles such names no matter what -
// and (2) at runtime the name is converted to the process locale, which drops
// or garbles non-ASCII bytes under the "C"/"POSIX" locale. We detect (1) and
// pick an installed UTF-8 locale for (2); when unzip lacks UNICODE_SUPPORT the
// non-ASCII name assertion is skipped (its round-trip is still covered by
// bsdtar/libarchive, which handles UTF-8 correctly regardless).
function findUtf8Locale() {
  const result = spawnSync('locale', ['-a'], { encoding: 'utf8' });
  if (result.status !== 0 || !result.stdout) return null;
  const locales = result.stdout.split('\n').map((line) => line.trim());
  const preferred = ['C.UTF-8', 'C.utf8', 'en_US.UTF-8', 'en_US.utf8'];
  for (const name of preferred) {
    if (locales.includes(name)) return name;
  }
  return locales.find((name) => /utf-?8$/i.test(name)) || null;
}

test('an archive written by createZipArchive is readable by unzip, zipinfo, and bsdtar', async (t) => {
  if (!hasTool('unzip', ['-v']) || !hasTool('zipinfo', ['-v']) || !hasTool('bsdtar', ['--version'])) {
    t.skip('unzip, zipinfo, or bsdtar is not available');
    return;
  }
  const locale = findUtf8Locale();
  if (!locale) {
    t.skip('no UTF-8 locale is available to render non-ASCII entry names');
    return;
  }
  const env = { ...process.env, LANG: locale, LC_ALL: locale };
  // Only assert non-ASCII names through zipinfo when this Info-ZIP build can
  // actually render them (see the note above).
  const unzipBuild = spawnSync('unzip', ['-v'], { encoding: 'utf8' }).stdout || '';
  const zipinfoUnicode = /UNICODE_SUPPORT/.test(unzipBuild);

  const dir = await fs.mkdtemp(path.join(tmpdir.path, 'zlib-zip-interop-'));
  try {
    const files = {
      'hello.txt': Buffer.from('Hello, world!'.repeat(50)),
      'raw.bin': Buffer.from([1, 2, 3, 4, 5]),
      'unicode-名前.txt': Buffer.from('unicode name'),
    };
    const entries = [];
    for (const [name, data] of Object.entries(files)) {
      entries.push(await zlib.ZipEntry.create(name, data));
    }
    const chunks = [];
    for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
    const archivePath = path.join(dir, 'archive.zip');
    await fs.writeFile(archivePath, Buffer.concat(chunks));

    const unzipTest = spawnSync('unzip', ['-t', archivePath], { env });
    assert.strictEqual(unzipTest.status, 0, unzipTest.stderr?.toString());

    const zipinfo = spawnSync('zipinfo', [archivePath], { env });
    assert.strictEqual(zipinfo.status, 0);
    for (const name of Object.keys(files)) {
      if (!zipinfoUnicode && /[^\x20-\x7e]/.test(name)) continue;
      assert.ok(zipinfo.stdout.toString().includes(name), `zipinfo missing ${name}`);
    }

    const extractDir = path.join(dir, 'out');
    await fs.mkdir(extractDir);
    const bsdtar = spawnSync('bsdtar', ['-xf', archivePath, '-C', extractDir], { env });
    assert.strictEqual(bsdtar.status, 0, bsdtar.stderr?.toString());
    for (const [name, data] of Object.entries(files)) {
      const extracted = await fs.readFile(path.join(extractDir, name));
      assert.deepStrictEqual(extracted, data);
    }
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
});

test('an archive written by Python\'s zipfile module is readable by ZipFile', async (t) => {
  if (!hasTool('python3', ['--version'])) {
    t.skip('python3 is not available');
    return;
  }

  const dir = await fs.mkdtemp(path.join(tmpdir.path, 'zlib-zip-interop-'));
  try {
    const archivePath = path.join(dir, 'python.zip');
    const script = `
import zipfile
with zipfile.ZipFile(${JSON.stringify(archivePath)}, 'w') as z:
    z.writestr('a.txt', 'hello from python')
    z.writestr('b.bin', bytes(range(256)))
`;
    const result = spawnSync('python3', ['-c', script]);
    assert.strictEqual(result.status, 0, result.stderr?.toString());

    const zip = await zlib.ZipFile.open(archivePath);
    try {
      const a = await zip.get('a.txt');
      assert.strictEqual((await a.content()).toString(), 'hello from python');
      const b = await zip.get('b.bin');
      assert.deepStrictEqual(await b.content(), Buffer.from(Array.from({ length: 256 }, (_, i) => i)));
    } finally {
      await zip.close();
    }
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
});
