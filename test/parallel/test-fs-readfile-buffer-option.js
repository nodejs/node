// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { internalBinding } = require('internal/test/binding');

const fsBinding = internalBinding('fs');

tmpdir.refresh();

const file = tmpdir.resolve('readfile-buffer-option.txt');
const content = Buffer.from('Hello buffer option\n'.repeat(128));
fs.writeFileSync(file, content);

function readFile(path, options) {
  return new Promise((resolve, reject) => {
    fs.readFile(path, options, (err, data) => {
      if (err) reject(err);
      else resolve(data);
    });
  });
}

function readFileFd(fd, options) {
  return new Promise((resolve, reject) => {
    fs.readFile(fd, options, common.mustCall((err, data) => {
      if (err) reject(err);
      else resolve(data);
    }));
  });
}

async function withFstatSizeZero(fn) {
  const originalFstat = fsBinding.fstat;
  fsBinding.fstat = function(...args) {
    const req = args[2];

    if (req?.oncomplete) {
      const originalOncomplete = req.oncomplete;
      req.oncomplete = function(err, stats) {
        if (!err) {
          stats[8] = 0;
        }
        return originalOncomplete.call(this, err, stats);
      };
      return Reflect.apply(originalFstat, this, args);
    }

    const stats = Reflect.apply(originalFstat, this, args);
    if (stats !== undefined) {
      stats[8] = 0;
    }
    return stats;
  };

  try {
    await fn();
  } finally {
    fsBinding.fstat = originalFstat;
  }
}

(async () => {
  {
    const buffer = Buffer.alloc(content.length + 16, 0x78);
    const data = fs.readFileSync(file, { buffer });
    assert.deepStrictEqual(data, buffer.subarray(0, content.length));
    assert.deepStrictEqual(data, content);
    assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
  }

  {
    const buffer = Buffer.alloc(content.length + 16);
    assert.strictEqual(
      fs.readFileSync(file, { buffer, encoding: 'utf8' }),
      content.toString('utf8'),
    );
    assert.deepStrictEqual(buffer.subarray(0, content.length), content);
  }

  {
    let size;
    const data = fs.readFileSync(file, {
      buffer(fileSize) {
        size = fileSize;
        return Buffer.alloc(fileSize + 8);
      },
    });
    assert.strictEqual(size, content.length);
    assert.deepStrictEqual(data, content);
  }

  {
    const buffer = Buffer.alloc(content.length + 16, 0x78);
    const data = await readFile(file, { buffer });
    assert.deepStrictEqual(data, buffer.subarray(0, content.length));
    assert.deepStrictEqual(data, content);
    assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
  }

  {
    const buffer = Buffer.alloc(content.length + 16);
    assert.strictEqual(
      await readFile(file, { buffer, encoding: 'utf8' }),
      content.toString('utf8'),
    );
    assert.deepStrictEqual(buffer.subarray(0, content.length), content);
  }

  {
    let size;
    const data = await readFile(file, {
      buffer(fileSize) {
        size = fileSize;
        return Buffer.alloc(fileSize + 8);
      },
    });
    assert.strictEqual(size, content.length);
    assert.deepStrictEqual(data, content);
  }

  {
    const buffer = Buffer.alloc(content.length - 1);
    assert.throws(() => fs.readFileSync(file, { buffer }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }

  await assert.rejects(readFile(file, {
    buffer() {
      return Buffer.alloc(content.length - 1);
    },
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });

  {
    const fd = fs.openSync(file, 'r');
    try {
      const buffer = Buffer.alloc(content.length - 1);
      await assert.rejects(readFileFd(fd, { buffer }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    } finally {
      fs.closeSync(fd);
    }
  }

  {
    const fd = fs.openSync(file, 'r');
    try {
      await assert.rejects(readFileFd(fd, {
        buffer() {
          return Buffer.alloc(content.length - 1);
        },
      }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    } finally {
      fs.closeSync(fd);
    }
  }

  await withFstatSizeZero(common.mustCall(async () => {
    {
      const buffer = Buffer.alloc(content.length + 16, 0x78);
      const data = fs.readFileSync(file, { buffer });
      assert.deepStrictEqual(data, buffer.subarray(0, content.length));
      assert.deepStrictEqual(data, content);
      assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
    }

    {
      const buffer = Buffer.alloc(content.length + 16, 0x78);
      const data = await readFile(file, { buffer });
      assert.deepStrictEqual(data, buffer.subarray(0, content.length));
      assert.deepStrictEqual(data, content);
      assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
    }

    {
      const buffer = Buffer.alloc(content.length - 1);
      assert.throws(() => fs.readFileSync(file, { buffer }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    }

    await assert.rejects(readFile(file, {
      buffer: Buffer.alloc(content.length - 1),
    }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }));
})().then(common.mustCall());
