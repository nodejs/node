'use strict';

const common = require('../common');
const assert = require('assert');
const {
  cp,
  cpSync,
  existsSync,
  mkdirSync,
  promises,
  readFileSync,
  readlinkSync,
  symlinkSync,
  writeFileSync,
} = require('fs');
const { Buffer } = require('buffer');
const { join, sep } = require('path');
const { pathToFileURL } = require('url');
const tmpdir = require('../common/tmpdir');

const sepBuffer = Buffer.from(sep);

function bufferPath(...parts) {
  const result = [];
  for (let i = 0; i < parts.length; i++) {
    if (i !== 0) result.push(sepBuffer);
    result.push(Buffer.isBuffer(parts[i]) ? parts[i] : Buffer.from(parts[i]));
  }
  return Buffer.concat(result);
}

function prepare(name) {
  const root = join(tmpdir.path, name);
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  mkdirSync(src, { recursive: true });
  writeFileSync(join(src, 'file.txt'), 'content');
  return { src, dest };
}

tmpdir.refresh();

{
  const root = join(tmpdir.path, 'buffer-files');
  const src = join(root, 'source.txt');
  const syncDest = join(root, 'sync.txt');
  const callbackDest = join(root, 'callback.txt');
  const promiseDest = join(root, 'promise.txt');
  mkdirSync(root, { recursive: true });
  writeFileSync(src, 'file-content');

  cpSync(Buffer.from(src), Buffer.from(syncDest));
  assert.strictEqual(readFileSync(syncDest, 'utf8'), 'file-content');

  cp(Buffer.from(src), Buffer.from(callbackDest), common.mustSucceed(() => {
    assert.strictEqual(readFileSync(callbackDest, 'utf8'), 'file-content');
  }));

  promises.cp(Buffer.from(src), Buffer.from(promiseDest)).then(common.mustCall(() => {
    assert.strictEqual(readFileSync(promiseDest, 'utf8'), 'file-content');
  }));
}

{
  const { src, dest } = prepare('sync-buffer');
  const seen = [];
  cpSync(Buffer.from(src), Buffer.from(dest), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert(Buffer.isBuffer(srcPath));
      assert(Buffer.isBuffer(destPath));
      seen.push([srcPath, destPath]);
      return true;
    }, 2),
  });
  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
  assert.strictEqual(seen.length, 2);
}

{
  const { src, dest } = prepare('sync-string');
  cpSync(src, dest, {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert.strictEqual(typeof srcPath, 'string');
      assert.strictEqual(typeof destPath, 'string');
      return true;
    }, 2),
  });
  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
}

{
  const { src, dest } = prepare('sync-url');
  cpSync(pathToFileURL(src), pathToFileURL(dest), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert.strictEqual(typeof srcPath, 'string');
      assert.strictEqual(typeof destPath, 'string');
      return true;
    }, 2),
  });
  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
}

{
  const root = join(tmpdir.path, 'sync-buffer-normalized');
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  mkdirSync(src, { recursive: true });
  writeFileSync(join(src, 'file.txt'), 'normalized');
  const seen = [];
  cpSync(Buffer.from(`${src}${sep}.${sep}`), Buffer.from(`${dest}${sep}.${sep}`), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      seen.push([srcPath, destPath]);
      return true;
    }, 2),
  });
  assert(seen[1][0].equals(Buffer.from(join(src, 'file.txt'))));
  assert(seen[1][1].equals(Buffer.from(join(dest, 'file.txt'))));
}

{
  const { src, dest } = prepare('sync-uint8array');
  cpSync(new Uint8Array(Buffer.from(src)), new Uint8Array(Buffer.from(dest)), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert(Buffer.isBuffer(srcPath));
      assert(Buffer.isBuffer(destPath));
      return true;
    }, 2),
  });
  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
}

{
  const { src, dest } = prepare('sync-mixed-buffer-string');
  cpSync(Buffer.from(src), dest, {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert(Buffer.isBuffer(srcPath));
      assert.strictEqual(typeof destPath, 'string');
      return true;
    }, 2),
  });
  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
}

{
  const { src, dest } = prepare('sync-mixed-string-buffer');
  cpSync(src, Buffer.from(dest), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert.strictEqual(typeof srcPath, 'string');
      assert(Buffer.isBuffer(destPath));
      return true;
    }, 2),
  });
  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
}

{
  const { src, dest } = prepare('callback-buffer');
  cp(Buffer.from(src), Buffer.from(dest), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert(Buffer.isBuffer(srcPath));
      assert(Buffer.isBuffer(destPath));
      return true;
    }, 2),
  }, common.mustSucceed(() => {
    assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
  }));
}

{
  const { src, dest } = prepare('promise-buffer');
  promises.cp(Buffer.from(src), Buffer.from(dest), {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      assert(Buffer.isBuffer(srcPath));
      assert(Buffer.isBuffer(destPath));
      return true;
    }, 2),
  }).then(common.mustCall(() => {
    assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'content');
  }));
}

if (common.isLinux) {
  const root = join(tmpdir.path, 'non-utf8');
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  const name = Buffer.from([
    0x82, 0xb1, 0x82, 0xf1, 0x82, 0xc9, 0x82,
    0xbf, 0x82, 0xcd, 0x90, 0x6c, 0x8a, 0x45,
  ]);
  mkdirSync(src, { recursive: true });
  const srcFile = bufferPath(src, name);
  const destFile = bufferPath(dest, name);
  writeFileSync(srcFile, 'byte-exact');

  let sawBytePath = false;
  cpSync(src, dest, {
    recursive: true,
    filter: common.mustCall((srcPath, destPath) => {
      if (Buffer.isBuffer(srcPath)) {
        assert(Buffer.isBuffer(destPath));
        assert(srcPath.equals(srcFile));
        assert(destPath.equals(destFile));
        sawBytePath = true;
      }
      return true;
    }, 2),
  });

  assert(sawBytePath);
  assert(existsSync(destFile));
  assert.strictEqual(readFileSync(destFile, 'utf8'), 'byte-exact');

  const urlSource = new URL(`${pathToFileURL(src).href}/${
    Array.from(name, (byte) => `%${byte.toString(16).padStart(2, '0')}`).join('')}`);
  const urlDest = new URL(`${pathToFileURL(dest).href}/url-copy`);
  cpSync(urlSource, urlDest);
  assert.strictEqual(readFileSync(join(dest, 'url-copy'), 'utf8'), 'byte-exact');
}


if (common.isLinux) {
  const invalidParent = Buffer.from([0x70, 0x61, 0xff, 0x72, 0x65, 0x6e, 0x74]);
  const invalidChild = Buffer.from([0x63, 0x68, 0xfe, 0x69, 0x6c, 0x64]);

  {
    const root = join(tmpdir.path, 'nested-non-utf8');
    const src = bufferPath(root, 'src');
    const dest = bufferPath(root, 'dest');
    const srcFile = bufferPath(src, invalidParent, invalidChild, 'file.txt');
    const destFile = bufferPath(dest, invalidParent, invalidChild, 'file.txt');
    mkdirSync(bufferPath(src, invalidParent, invalidChild), { recursive: true });
    writeFileSync(srcFile, 'nested-byte-exact');

    cpSync(src, dest, { recursive: true });
    assert.strictEqual(readFileSync(destFile, 'utf8'), 'nested-byte-exact');
  }

  {
    const root = join(tmpdir.path, 'mixed-path-types');
    const src = join(root, 'src');
    const dest = join(root, 'dest');
    const srcFile = bufferPath(src, invalidParent, 'file.txt');
    const destFile = bufferPath(dest, invalidParent, 'file.txt');
    mkdirSync(bufferPath(src, invalidParent), { recursive: true });
    writeFileSync(srcFile, 'mixed-types');

    const seen = [];
    cpSync(Buffer.from(src), dest, {
      recursive: true,
      filter: common.mustCall((srcPath, destPath) => {
        seen.push([srcPath, destPath]);
        return true;
      }, 3),
    });

    assert.strictEqual(typeof seen[0][0], 'object');
    assert.strictEqual(typeof seen[0][1], 'string');
    assert(Buffer.isBuffer(seen[1][0]));
    assert(Buffer.isBuffer(seen[1][1]));
    assert.strictEqual(readFileSync(destFile, 'utf8'), 'mixed-types');
  }

  {
    const root = join(tmpdir.path, 'self-copy-non-utf8');
    const src = bufferPath(root, invalidParent);
    const dest = bufferPath(src, invalidChild);
    mkdirSync(src, { recursive: true });
    writeFileSync(bufferPath(src, 'file.txt'), 'self-copy');

    assert.throws(
      () => cpSync(src, dest, { recursive: true }),
      { code: 'ERR_FS_CP_EINVAL' },
    );
  }

  {
    const root = join(tmpdir.path, 'callback-nested-non-utf8');
    const src = bufferPath(root, 'src');
    const dest = bufferPath(root, 'dest');
    const destFile = bufferPath(dest, invalidParent, invalidChild, 'file.txt');
    mkdirSync(bufferPath(src, invalidParent, invalidChild), { recursive: true });
    writeFileSync(bufferPath(src, invalidParent, invalidChild, 'file.txt'), 'callback-bytes');

    cp(src, dest, {
      recursive: true,
      filter: common.mustCall((srcPath, destPath) => {
        assert(Buffer.isBuffer(srcPath));
        assert(Buffer.isBuffer(destPath));
        return true;
      }, 4),
    }, common.mustSucceed(() => {
      assert.strictEqual(readFileSync(destFile, 'utf8'), 'callback-bytes');
    }));
  }

  {
    const root = join(tmpdir.path, 'promise-nested-non-utf8');
    const src = bufferPath(root, 'src');
    const dest = bufferPath(root, 'dest');
    const destFile = bufferPath(dest, invalidParent, invalidChild, 'file.txt');
    mkdirSync(bufferPath(src, invalidParent, invalidChild), { recursive: true });
    writeFileSync(bufferPath(src, invalidParent, invalidChild, 'file.txt'), 'promise-bytes');

    promises.cp(src, dest, {
      recursive: true,
      filter: common.mustCall((srcPath, destPath) => {
        assert(Buffer.isBuffer(srcPath));
        assert(Buffer.isBuffer(destPath));
        return true;
      }, 4),
    }).then(common.mustCall(() => {
      assert.strictEqual(readFileSync(destFile, 'utf8'), 'promise-bytes');
    }));
  }

  {
    const root = join(tmpdir.path, 'url-non-utf8-all-apis');
    const src = join(root, 'src');
    const dest = join(root, 'dest');
    mkdirSync(src, { recursive: true });
    const name = Buffer.from([0x66, 0x80, 0x6f]);
    writeFileSync(bufferPath(src, name), 'url-bytes');
    const encodedName = Array.from(
      name,
      (byte) => `%${byte.toString(16).padStart(2, '0')}`,
    ).join('');
    const source = new URL(`${pathToFileURL(src).href}/${encodedName}`);
    const callbackDest = new URL(`${pathToFileURL(dest).href}/callback`);
    const promiseDest = new URL(`${pathToFileURL(dest).href}/promise`);

    cp(source, callbackDest, {
      filter: common.mustCall((srcPath, destPath) => {
        assert(Buffer.isBuffer(srcPath));
        assert.strictEqual(typeof destPath, 'string');
        return true;
      }),
    }, common.mustSucceed(() => {
      assert.strictEqual(readFileSync(join(dest, 'callback'), 'utf8'), 'url-bytes');
    }));
    promises.cp(source, promiseDest, {
      filter: common.mustCall((srcPath, destPath) => {
        assert(Buffer.isBuffer(srcPath));
        assert.strictEqual(typeof destPath, 'string');
        return true;
      }),
    }).then(common.mustCall(() => {
      assert.strictEqual(readFileSync(join(dest, 'promise'), 'utf8'), 'url-bytes');
    }));
  }

  {
    const root = join(tmpdir.path, 'url-non-utf8-destinations');
    const src = join(root, 'source.txt');
    mkdirSync(root, { recursive: true });
    writeFileSync(src, 'destination-url-bytes');
    const names = [
      Buffer.from([0x73, 0x79, 0x6e, 0x63, 0x80]),
      Buffer.from([0x63, 0x61, 0x6c, 0x6c, 0x62, 0x61, 0x63, 0x6b, 0x81]),
      Buffer.from([0x70, 0x72, 0x6f, 0x6d, 0x69, 0x73, 0x65, 0x82]),
    ];
    const urls = names.map((name) => new URL(
      Array.from(
        name,
        (byte) => `%${byte.toString(16).padStart(2, '0')}`,
      ).join(''),
      pathToFileURL(`${root}${sep}`),
    ));

    cpSync(src, urls[0], {
      filter: common.mustCall((srcPath, destPath) => {
        assert.strictEqual(typeof srcPath, 'string');
        assert(Buffer.isBuffer(destPath));
        return true;
      }),
    });
    assert.strictEqual(readFileSync(bufferPath(root, names[0]), 'utf8'), 'destination-url-bytes');

    cp(src, urls[1], {
      filter: common.mustCall((srcPath, destPath) => {
        assert.strictEqual(typeof srcPath, 'string');
        assert(Buffer.isBuffer(destPath));
        return true;
      }),
    }, common.mustSucceed(() => {
      assert.strictEqual(readFileSync(bufferPath(root, names[1]), 'utf8'), 'destination-url-bytes');
    }));

    promises.cp(src, urls[2], {
      filter: common.mustCall((srcPath, destPath) => {
        assert.strictEqual(typeof srcPath, 'string');
        assert(Buffer.isBuffer(destPath));
        return true;
      }),
    }).then(common.mustCall(() => {
      assert.strictEqual(readFileSync(bufferPath(root, names[2]), 'utf8'), 'destination-url-bytes');
    }));
  }

  {
    const root = join(tmpdir.path, 'async-self-copy-non-utf8');
    const callbackSrc = bufferPath(root, 'callback', invalidParent);
    const callbackDest = bufferPath(callbackSrc, invalidChild);
    mkdirSync(callbackSrc, { recursive: true });
    writeFileSync(bufferPath(callbackSrc, 'file.txt'), 'callback-self-copy');
    cp(callbackSrc, callbackDest, { recursive: true }, common.mustCall((err) => {
      assert.strictEqual(err?.code, 'ERR_FS_CP_EINVAL');
    }));

    const promiseSrc = bufferPath(root, 'promise', invalidParent);
    const promiseDest = bufferPath(promiseSrc, invalidChild);
    mkdirSync(promiseSrc, { recursive: true });
    writeFileSync(bufferPath(promiseSrc, 'file.txt'), 'promise-self-copy');
    assert.rejects(
      promises.cp(promiseSrc, promiseDest, { recursive: true }),
      { code: 'ERR_FS_CP_EINVAL' },
    ).then(common.mustCall());
  }

  {
    const root = join(tmpdir.path, 'symlink-non-utf8');
    const srcDir = bufferPath(root, 'src');
    const destDir = bufferPath(root, 'dest');
    mkdirSync(srcDir, { recursive: true });
    const target = invalidChild;
    writeFileSync(bufferPath(srcDir, target), 'symlink-target');
    symlinkSync(target, bufferPath(srcDir, 'link'));

    cpSync(srcDir, destDir, { recursive: true, verbatimSymlinks: true });
    assert(readlinkSync(bufferPath(destDir, 'link'), { encoding: 'buffer' }).equals(target));

    const resolvedDestDir = bufferPath(root, 'resolved-dest');
    cpSync(srcDir, resolvedDestDir, { recursive: true });
    assert(
      readlinkSync(bufferPath(resolvedDestDir, 'link'), { encoding: 'buffer' })
        .equals(bufferPath(srcDir, target)),
    );
  }
}


if (!common.isWindows) {
  const root = join(tmpdir.path, 'verbatim-existing-relative-symlink');
  const target = join(root, 'target');
  mkdirSync(join(target, 'child'), { recursive: true });

  const src = join(root, 'src');
  const dest = join(root, 'dest');
  symlinkSync('target/child', src);
  symlinkSync('target', dest);
  assert.rejects(
    promises.cp(Buffer.from(src), Buffer.from(dest), {
      verbatimSymlinks: true,
    }),
    { code: 'ERR_FS_CP_SYMLINK_TO_SUBDIRECTORY' },
  ).then(common.mustCall());
}

if (common.isLinux) {
  const root = join(tmpdir.path, 'absolute-symlink-non-utf8');
  const srcDir = bufferPath(root, 'src');
  const destDir = bufferPath(root, 'dest');
  const invalidName = Buffer.from([0x74, 0x61, 0x72, 0xff, 0x67, 0x65, 0x74]);
  const absoluteTarget = bufferPath(root, 'unused', '..', invalidName);

  mkdirSync(srcDir, { recursive: true });
  symlinkSync(absoluteTarget, bufferPath(srcDir, 'link'));
  cpSync(srcDir, destDir, { recursive: true });
  assert(
    readlinkSync(bufferPath(destDir, 'link'), { encoding: 'buffer' })
      .equals(absoluteTarget),
  );
}


if (common.isLinux) {
  const root = join(tmpdir.path, 'unicode-\u03c0-parent');
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  const invalidName = Buffer.from([0x66, 0x69, 0xff, 0x6c, 0x65]);
  mkdirSync(src, { recursive: true });
  writeFileSync(bufferPath(src, invalidName), 'unicode-parent');

  cpSync(src, dest, { recursive: true });
  assert.strictEqual(
    readFileSync(bufferPath(dest, invalidName), 'utf8'),
    'unicode-parent',
  );
}
