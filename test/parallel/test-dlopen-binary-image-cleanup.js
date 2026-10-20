// Flags: --expose-internals
'use strict';

// Loading an addon from bytes materializes them into a private image so the
// dynamic loader has a real path to open. That image is transient and must not
// outlive the process that loaded it. How it is held differs by platform, so
// this checks both halves of the contract:
//
//   Linux:       an anonymous memfd loaded through /proc/self/fd - nothing ever
//                reaches the filesystem, and AfterOpen() closes the descriptor
//                once the load owns its mapping, so repeated loads must not
//                accumulate open descriptors.
//   other POSIX: a mkdtemp() directory unlinked and rmdir()ed right after the
//                load, so nothing is left even while the process runs.
//   Windows:     the loader maps the file by path for the DLL's lifetime, so
//                the image has to stay put; it is retained with the module it
//                loaded as, and both are released at process exit.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

const addonPath = path.join(
  __dirname, '..', 'addons', 'hello-world', 'build', 'Release', 'binding.node');
if (!fs.existsSync(addonPath)) common.skip('the hello-world addon is not built');

tmpdir.refresh();

// Where a temp-file image would land: GetTempPathW() reads TMP/TEMP and
// TempDir() reads TMPDIR, so pointing all three at a directory this test owns
// keeps any image the child writes somewhere it can inspect afterwards. Linux
// normally uses a memfd and never writes here at all.
const imageDir = tmpdir.resolve('addon-images');
fs.mkdirSync(imageDir, { recursive: true });

const child = `
  const fs = require('fs');
  const { internalBinding } = require('internal/test/binding');
  const { dlopenBinary } = internalBinding('process_methods');
  const bytes = fs.readFileSync(${JSON.stringify(addonPath)});
  // A path that does not exist on disk, as a VFS-resident addon would be, so
  // the load can only come from the bytes and their materialized image.
  const virtualPath = ${JSON.stringify(path.join(addonPath, '..', 'nowhere', 'binding.node'))};

  // On Linux the image is a descriptor rather than a file, so count them: each
  // load must hand its fd to the mapping and close it, leaving no growth.
  const fdDir = '/proc/self/fd';
  const countFds = () => {
    try { return fs.readdirSync(fdDir).length; } catch { return -1; }
  };
  const before = countFds();

  // Load repeatedly: each load materializes its own image, so a leak of an
  // image, a descriptor or a retained handle shows up as growth.
  for (let i = 0; i < 5; i++) {
    const m = { exports: {} };
    // flags undefined: keep the default dlopen(2) mode - 0 is not valid
    // everywhere (glibc rejects it with EINVAL).
    dlopenBinary(m, virtualPath, undefined, bytes);
    if (m.exports.hello() !== 'world') throw new Error('addon did not load');
  }

  const after = countFds();
  if (before !== -1 && after > before) {
    throw new Error(\`descriptor leak: \${before} -> \${after} after 5 loads\`);
  }
  process.exit(0);
`;

const res = spawnSync(process.execPath, ['--expose-internals', '-e', child], {
  env: { ...process.env, TMPDIR: imageDir, TMP: imageDir, TEMP: imageDir },
  encoding: 'utf8',
});

// The load itself must succeed. On Windows a retained writable handle makes the
// loader fail with ERROR_SHARING_VIOLATION ("The process cannot access the file
// because it is being used by another process").
assert.strictEqual(res.status, 0, `child failed:\n${res.stderr}`);

// Nothing an image left behind may outlive the process that created it. Match
// the shapes the two on-disk paths produce rather than requiring the directory
// to be empty, so an unrelated temp file cannot fail this.
const leftovers = fs.readdirSync(imageDir).filter(
  (name) => /^nod.*\.tmp$/i.test(name) || name.startsWith('node-addon-'));
assert.deepStrictEqual(
  leftovers, [],
  `materialized addon image outlived the process that loaded it: ${leftovers}`);
