'use strict';

// This tests that fs.cp, fs.promises.cp, and fs.cpSync all accept Buffer
// paths for `src` and `dest` (matching the documented `string | Buffer | URL`
// type). Prior to the fix for https://github.com/nodejs/node/issues/58869,
// the async variants threw `ERR_INVALID_ARG_TYPE` because the internal
// implementation called `path.resolve` on a Buffer, while the sync variant
// already accepted Buffer paths via the C++ binding.

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { Buffer } = require('node:buffer');
const { pathToFileURL } = require('node:url');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

let counter = 0;
function makePaths(prefix) {
  const id = `${prefix}-${process.pid}-${++counter}`;
  return {
    src: tmpdir.resolve(`${id}-src.txt`),
    dest: tmpdir.resolve(`${id}-dest.txt`),
  };
}

function writeSrc(srcPath) {
  fs.writeFileSync(srcPath, 'cp-buffer-path');
}

function assertCopied(srcPath, destPath) {
  assert.strictEqual(
    fs.readFileSync(destPath, 'utf8'),
    fs.readFileSync(srcPath, 'utf8'),
  );
}

(async () => {
  // 1. fs.promises.cp() accepts Buffer src and Buffer dest. This used to
  //    throw ERR_INVALID_ARG_TYPE.
  {
    const { src, dest } = makePaths('promises-buf');
    writeSrc(src);
    await fs.promises.cp(Buffer.from(src), Buffer.from(dest));
    assertCopied(src, dest);
  }

  // 2. Mixed forms: Buffer src + string dest, and string src + Buffer dest.
  {
    const { src, dest } = makePaths('promises-mixed1');
    writeSrc(src);
    await fs.promises.cp(Buffer.from(src), dest);
    assertCopied(src, dest);
  }

  {
    const { src, dest } = makePaths('promises-mixed2');
    writeSrc(src);
    await fs.promises.cp(src, Buffer.from(dest));
    assertCopied(src, dest);
  }

  // 3. fs.cpSync() continues to accept Buffer src and Buffer dest (the
  //    existing documented behavior); guard against regressions there too.
  {
    const { src, dest } = makePaths('sync-buf');
    writeSrc(src);
    fs.cpSync(Buffer.from(src), Buffer.from(dest));
    assertCopied(src, dest);
  }

  // 4. URL paths still work for the async variants.
  {
    const { src, dest } = makePaths('promises-url');
    writeSrc(src);
    await fs.promises.cp(pathToFileURL(src), pathToFileURL(dest));
    assertCopied(src, dest);
  }

  // 5. Recursive directory copy with Buffer paths via fs.promises.cp.
  {
    const id = `dir-${process.pid}-${++counter}`;
    const srcDir = tmpdir.resolve(`${id}-src`);
    const destDir = tmpdir.resolve(`${id}-dest`);
    fs.mkdirSync(srcDir, { recursive: true });
    fs.writeFileSync(path.join(srcDir, 'a.txt'), 'a');
    fs.writeFileSync(path.join(srcDir, 'b.txt'), 'b');
    await fs.promises.cp(Buffer.from(srcDir), Buffer.from(destDir),
                         { recursive: true });
    assert.strictEqual(fs.readFileSync(path.join(destDir, 'a.txt'), 'utf8'), 'a');
    assert.strictEqual(fs.readFileSync(path.join(destDir, 'b.txt'), 'utf8'), 'b');
  }

  // 6. fs.cp() (callback) accepts Buffer src and Buffer dest. Since fs.cp is
  //    a callbackified version of fs.promises.cp, this exercises the same fix.
  {
    const { src, dest } = makePaths('cb-buf');
    writeSrc(src);
    fs.cp(Buffer.from(src), Buffer.from(dest), common.mustSucceed(() => {
      assertCopied(src, dest);
    }));
  }
})().then(common.mustCall());
