// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const {
  open,
  readFile,
} = fs.promises;
const tmpdir = require('../common/tmpdir');
const { internalBinding } = require('internal/test/binding');

const fsBinding = internalBinding('fs');

tmpdir.refresh();

const file = tmpdir.resolve('fs-promises-readfile-buffer-option.txt');
const content = Buffer.from('Hello promises buffer option\n'.repeat(128));
fs.writeFileSync(file, content);

async function withFstatSizeZero(fn) {
  const originalFstat = fsBinding.fstat;
  fsBinding.fstat = function(...args) {
    const result = Reflect.apply(originalFstat, this, args);
    return Promise.resolve(result).then((stats) => {
      if (stats !== undefined) {
        stats[8] = 0;
      }
      return stats;
    });
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
    await using handle = await open(file, 'r');
    const buffer = Buffer.alloc(content.length + 16, 0x78);
    const data = await handle.readFile({ buffer });
    assert.deepStrictEqual(data, buffer.subarray(0, content.length));
    assert.deepStrictEqual(data, content);
    assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
  }

  {
    await using handle = await open(file, 'r');
    let size;
    const data = await handle.readFile({
      buffer(fileSize) {
        size = fileSize;
        return Buffer.alloc(fileSize + 8);
      },
    });
    assert.strictEqual(size, content.length);
    assert.deepStrictEqual(data, content);
  }


  await assert.rejects(readFile(file, {
    buffer() {
      return Buffer.alloc(content.length - 1);
    },
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });

  {
    await using handle = await open(file, 'r');
    await assert.rejects(handle.readFile({
      buffer: Buffer.alloc(content.length - 1),
    }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }

  await withFstatSizeZero(common.mustCall(async () => {
    {
      const buffer = Buffer.alloc(content.length + 16, 0x78);
      const data = await readFile(file, { buffer });
      assert.deepStrictEqual(data, buffer.subarray(0, content.length));
      assert.deepStrictEqual(data, content);
      assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
    }

    {
      await using handle = await open(file, 'r');
      const buffer = Buffer.alloc(content.length + 16, 0x78);
      const data = await handle.readFile({ buffer });
      assert.deepStrictEqual(data, buffer.subarray(0, content.length));
      assert.deepStrictEqual(data, content);
      assert(buffer.subarray(content.length).every((byte) => byte === 0x78));
    }

    await assert.rejects(readFile(file, {
      buffer: Buffer.alloc(content.length - 1),
    }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });

    {
      await using handle = await open(file, 'r');
      await assert.rejects(handle.readFile({
        buffer: Buffer.alloc(content.length - 1),
      }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    }
  }));
})().then(common.mustCall());
