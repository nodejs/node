// Flags: --expose-internals
'use strict';

// Loading an addon from bytes materializes them into a private image so the
// dynamic loader has a real path to open. That image is transient and must not
// outlive the process that loaded it. How it is held differs by platform, so
// this checks both halves of the contract:
//
//   Linux:       an anonymous memfd loaded through /proc/self/fd - nothing ever
//                reaches the filesystem. A loaded memfd stays open until exit,
//                close-on-exec, so that no later image is loaded through the
//                same /proc/self/fd/N path (the loader would return this one);
//                a failed load closes its memfd again.
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

  // On Linux the image is a memfd rather than a file, so count those: each
  // successful load keeps exactly one, close-on-exec, and a failed load none.
  const fdDir = '/proc/self/fd';
  const imageFds = () => {
    let fds;
    try { fds = fs.readdirSync(fdDir); } catch { return null; }
    return fds.filter((fd) => {
      try {
        return fs.readlinkSync(\`\${fdDir}/\${fd}\`).startsWith('/memfd:node-addon');
      } catch { return false; }
    });
  };
  const before = imageFds();

  // Load repeatedly: each load materializes its own image, so a leak of an
  // image or a retained handle shows up as growth.
  const loads = 5;
  for (let i = 0; i < loads; i++) {
    const m = { exports: {} };
    // flags undefined: keep the default dlopen(2) mode - 0 is not valid
    // everywhere (glibc rejects it with EINVAL).
    dlopenBinary(m, virtualPath, undefined, bytes);
    if (m.exports.hello() !== 'world') throw new Error('addon did not load');
  }
  const loaded = imageFds();

  for (let i = 0; i < loads; i++) {
    try {
      dlopenBinary({ exports: {} }, virtualPath, undefined, Buffer.from('nope'));
      throw new Error('loading garbage bytes did not fail');
    } catch (err) {
      if (err.code !== 'ERR_DLOPEN_FAILED') throw err;
    }
  }
  const failed = imageFds();

  if (before !== null) {
    const kept = loaded.filter((fd) => !before.includes(fd));
    // Zero when memfd is unavailable and the temp-file fallback was used.
    if (kept.length !== 0 && kept.length !== loads) {
      throw new Error(\`expected \${loads} retained images, got \${kept.length}\`);
    }
    if (failed.length !== loaded.length) {
      throw new Error(\`failed loads kept images: \${loaded} -> \${failed}\`);
    }
    for (const fd of kept) {
      const info = fs.readFileSync(\`/proc/self/fdinfo/\${fd}\`, 'utf8');
      const flags = parseInt(/^flags:\\s*([0-7]+)/m.exec(info)[1], 8);
      if ((flags & 0o2000000) === 0) {  // O_CLOEXEC
        throw new Error(\`retained image fd \${fd} is not close-on-exec\`);
      }
    }
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
