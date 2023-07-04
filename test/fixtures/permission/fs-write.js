'use strict';

const common = require('../../common');
common.skipIfWorker();

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const regularFolder = process.env.ALLOWEDFOLDER;
const regularFile = process.env.ALLOWEDFILE;
const blockedFolder = process.env.BLOCKEDFOLDER;
const blockedFile = process.env.BLOCKEDFILE;
const relativeProtectedFile = process.env.RELATIVEBLOCKEDFILE;
const relativeProtectedFolder = process.env.RELATIVEBLOCKEDFOLDER;
const absoluteProtectedFile = path.resolve(relativeProtectedFile);
const absoluteProtectedFolder = path.resolve(relativeProtectedFolder);

{
  assert.ok(!process.permission.has('fs.write', blockedFolder));
  assert.ok(!process.permission.has('fs.write', blockedFile));
}

// fs.writeFile
{
  assert.throws(() => {
    fs.writeFile(blockedFile, 'example', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.writeFile(relativeProtectedFile, 'example', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));

  assert.throws(() => {
    fs.writeFile(path.join(blockedFolder, 'anyfile'), 'example', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'anyfile')),
  }));
}

// fs.createWriteStream
{
  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createWriteStream(blockedFile);
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  })).then(common.mustCall());
  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createWriteStream(relativeProtectedFile);
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  })).then(common.mustCall());

  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createWriteStream(path.join(blockedFolder, 'example'));
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'example')),
  })).then(common.mustCall());
}

// fs.utimes
{
  assert.throws(() => {
    fs.utimes(blockedFile, new Date(), new Date(), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.utimes(relativeProtectedFile, new Date(), new Date(), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));

  assert.throws(() => {
    fs.utimes(path.join(blockedFolder, 'anyfile'), new Date(), new Date(), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'anyfile')),
  }));
}

// fs.lutimes
{
  assert.throws(() => {
    fs.lutimes(blockedFile, new Date(), new Date(), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  }));
}

// fs.mkdir
{
  assert.throws(() => {
    fs.mkdir(path.join(blockedFolder, 'any-folder'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'any-folder')),
  }));
  assert.throws(() => {
    fs.mkdir(path.join(relativeProtectedFolder, 'any-folder'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(absoluteProtectedFolder, 'any-folder')),
  }));
}

{
  assert.throws(() => {
    fs.mkdtempSync(path.join(blockedFolder, 'any-folder'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  }));
  assert.throws(() => {
    fs.mkdtemp(path.join(relativeProtectedFolder, 'any-folder'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  }));
}

// fs.rename
{
  assert.throws(() => {
    fs.rename(blockedFile, path.join(blockedFile, 'renamed'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.rename(relativeProtectedFile, path.join(relativeProtectedFile, 'renamed'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
  assert.throws(() => {
    fs.rename(blockedFile, path.join(regularFolder, 'renamed'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  }));

  assert.throws(() => {
    fs.rename(regularFile, path.join(blockedFolder, 'renamed'), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'renamed')),
  }));
}

// fs.copyFile
{
  assert.throws(() => {
    fs.copyFileSync(regularFile, path.join(blockedFolder, 'any-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'any-file')),
  }));
  assert.throws(() => {
    fs.copyFileSync(regularFile, path.join(relativeProtectedFolder, 'any-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(absoluteProtectedFolder, 'any-file')),
  }));
}

// fs.cp
{
  assert.throws(() => {
    fs.cpSync(regularFile, path.join(blockedFolder, 'any-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'any-file')),
  }));
  assert.throws(() => {
    fs.cpSync(regularFile, path.join(relativeProtectedFolder, 'any-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(absoluteProtectedFolder, 'any-file')),
  }));
}

// fs.rm
{
  assert.throws(() => {
    fs.rmSync(blockedFolder, { recursive: true });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFolder),
  }));
  assert.throws(() => {
    fs.rmSync(relativeProtectedFolder, { recursive: true });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(absoluteProtectedFolder),
  }));
}

// fs.open
{
  // Extra flags should not enable trivially bypassing all restrictions.
  // See https://github.com/nodejs/node/issues/47090.
  assert.throws(() => {
    fs.open(blockedFile, fs.constants.O_RDWR | 0x10000000, common.mustNotCall());
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  assert.rejects(async () => {
    await fs.promises.open(blockedFile, fs.constants.O_RDWR | fs.constants.O_NOFOLLOW);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  if (common.isWindows) {
    // In particular, on Windows, the permission system should not blindly let
    // code delete write-protected files.
    const O_TEMPORARY = 0x40;
    assert.throws(() => {
      fs.openSync(blockedFile, fs.constants.O_RDONLY | O_TEMPORARY);
    }, {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemWrite'
    });
  }
}

// fs.chmod
{
  assert.throws(() => {
    fs.chmod(blockedFile, 0o755, common.mustNotCall());
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  assert.rejects(async () => {
    await fs.promises.chmod(blockedFile, 0o755);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
}

// fs.lchmod
{
  if (common.isOSX) {
    assert.throws(() => {
      fs.lchmod(blockedFile, 0o755, common.mustNotCall());
    }, {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemWrite',
    });
    assert.rejects(async () => {
      await fs.promises.lchmod(blockedFile, 0o755);
    }, {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemWrite',
    });
  }
}

// fs.appendFile
{
  assert.throws(() => {
    fs.appendFile(blockedFile, 'new data', common.mustNotCall());
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  assert.rejects(async () => {
    await fs.promises.appendFile(blockedFile, 'new data');
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
}

// fs.chown
{
  assert.throws(() => {
    fs.chown(blockedFile, 1541, 999, common.mustNotCall());
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  assert.rejects(async () => {
    await fs.promises.chown(blockedFile, 1541, 999);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
}

// fs.lchown
{
  assert.throws(() => {
    fs.lchown(blockedFile, 1541, 999, common.mustNotCall());
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  assert.rejects(async () => {
    await fs.promises.lchown(blockedFile, 1541, 999);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
}

// fs.link
{
  assert.throws(() => {
    fs.link(blockedFile, path.join(blockedFolder, '/linked'), common.mustNotCall());
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
  assert.rejects(async () => {
    await fs.promises.link(blockedFile, path.join(blockedFolder, '/linked'));
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  });
}

// fs.unlink
{
  assert.throws(() => {
    fs.unlink(blockedFile, (err) => {
      assert.ifError(err);
    });
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(blockedFile),
  });
}
